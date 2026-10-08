#!/usr/bin/env python3
"""Equal-byte B2L/B3L/B4 matched-cell runner. Stdlib only."""

import argparse
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

from phase_schema import parse_phase_line, validate_v2_reconciliation

BASELINES = ("B2L", "B3L", "B4")
TELEMETRY_FIELDS = (
    "schema_version", "oracle_ns", "algorithm_ns", "query_ns", "update_ns",
    "operation_p50_ns", "operation_p95_ns", "query_p50_ns", "query_p95_ns",
    "update_p50_ns", "update_p95_ns", "cooldown_blocks", "admission_rejections",
    "filtered_updates", "decrease_repairs", "increase_repairs",
    "memory_budget_rejections", "memory_metadata_prunes", "b4_query_time_ns",
    "b4_update_time_ns", "b4_cold_query_ns", "b4_resident_query_ns",
    "b4_query_policy_ns", "b4_promotion_ns", "b4_eviction_ns",
    "b4_graph_update_ns", "b4_decrease_repair_ns", "b4_increase_repair_ns",
    "b4_rebuild_ns", "b4_controller_ns", "b4_update_policy_ns",
)


def positive(value):
    n = int(value)
    if n <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return n


def nonnegative(value):
    n = int(value)
    if n < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return n


def run_one(binary, graph, baseline, family, queries, update_every,
            hot_sources, epoch, seed, update_mode, budget, timeout, time_file):
    env = os.environ.copy()
    env["ADES_PERSISTENT_STATE_BUDGET_BYTES"] = str(budget)
    cmd = [
        str(binary), str(graph), baseline, family, str(queries), str(update_every),
        str(hot_sources), str(epoch), str(seed), "1", update_mode,
    ]
    started = dt.datetime.now(dt.timezone.utc).isoformat()
    t0 = time.monotonic()
    try:
        proc = subprocess.run(
            ["/usr/bin/time", "-f", "%M", "-o", str(time_file), *cmd],
            capture_output=True, text=True, timeout=timeout, check=False, env=env,
        )
        rc, stdout, stderr = proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired as exc:
        rc = 124
        stdout = ((exc.stdout or b"").decode(errors="replace")
                  if isinstance(exc.stdout, bytes) else (exc.stdout or ""))
        stderr = ((exc.stderr or b"").decode(errors="replace")
                  if isinstance(exc.stderr, bytes) else (exc.stderr or ""))
        stderr += "\nTIMEOUT\n"
    return started, time.monotonic() - t0, rc, stdout, stderr


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", default="./build/ades_phase")
    p.add_argument("--graph", required=True)
    p.add_argument("--output", default="results/equal-byte")
    p.add_argument("--budgets-bytes", type=positive, nargs="+", required=True)
    p.add_argument("--queries", type=positive, default=100)
    p.add_argument("--repeats", type=positive, default=2)
    p.add_argument("--seeds", type=int, nargs="+", default=[7, 17])
    p.add_argument("--families", nargs="+", choices=[
        "uniform", "zipf", "single-hot", "rotating-hot", "hot-pool", "churn"
    ], default=["single-hot", "hot-pool"])
    p.add_argument("--update-every", type=nonnegative, nargs="+", default=[0, 10])
    p.add_argument("--update-mode", choices=["random", "alternating"], default="alternating")
    p.add_argument("--hot-sources", type=positive, default=4)
    p.add_argument("--epoch", type=positive, default=250)
    p.add_argument("--timeout", type=positive, default=300)
    args = p.parse_args()

    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)
    binary = Path(args.binary).resolve()
    graph = Path(args.graph).resolve()
    if not binary.is_file() or not graph.is_file():
        p.error("binary and graph must exist")

    manifest = {
        "schema": "ades-pr44-equal-byte-v2",
        "phase_schema_version": 2,
        "claim_ids": ["C1", "C2", "C3"],
        "utc_started": dt.datetime.now(dt.timezone.utc).isoformat(),
        "git_commit": os.environ.get("GITHUB_SHA", "local-unpinned"),
        "graph": str(graph),
        "graph_sha256": hashlib.sha256(graph.read_bytes()).hexdigest(),
        "binary": str(binary),
        "platform": platform.platform(),
        "python": sys.version,
        "runner": os.environ.get("RUNNER_NAME", "local"),
        "comparison_class": "equal logical persistent-state byte budget; source-count ceiling nonbinding",
        "memory_metric": "accounted_algorithm_state_bytes; RSS reported separately",
        "trace_identity": "canonical operation stream SHA-256",
        "timing_contract": "oracle excluded; algorithm_ns=query_ns+update_ns; B4 components disjoint",
        "config": {k: v for k, v in vars(args).items()
                   if k not in ("binary", "graph", "output")},
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    fields = [
        "cell", "repeat", "order", "baseline", "family", "seed", "queries",
        "update_every", "update_mode", "hot_sources", "epoch", "budget_bytes",
        "trace_sha256", "updates", "increase_count", "decrease_count",
        "source_cap", "accounted_algorithm_state_bytes",
        "peak_accounted_algorithm_state_bytes", "accounting_version",
        *TELEMETRY_FIELDS,
        "cold_queries", "resident_queries", "promotions", "evictions", "rebuilds",
        "repair_aborts", "peak_child_rss_kb", "elapsed_wall_s", "start_utc",
        "exit_code", "claim_ids",
    ]

    failed = False
    cell = 0
    with (out / "measurements.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for budget in args.budgets_bytes:
            for seed in args.seeds:
                for family in args.families:
                    for ue in args.update_every:
                        cell += 1
                        expected_signature = None
                        expected_accounting_version = None
                        for rep in range(args.repeats):
                            rotation = (rep + cell - 1) % len(BASELINES)
                            order = BASELINES[rotation:] + BASELINES[:rotation]
                            for order_index, baseline in enumerate(order):
                                key = f"cell{cell:03d}-rep{rep:02d}-{baseline}"
                                time_file = out / f"{key}.time"
                                started, wall, rc, stdout, stderr = run_one(
                                    binary, graph, baseline, family, args.queries, ue,
                                    args.hot_sources, args.epoch, seed, args.update_mode,
                                    budget, args.timeout, time_file,
                                )
                                (out / f"{key}.stdout").write_text(stdout)
                                (out / f"{key}.stderr").write_text(stderr)
                                row = {
                                    "cell": cell, "repeat": rep, "order": order_index,
                                    "baseline": baseline, "family": family, "seed": seed,
                                    "queries": args.queries, "update_every": ue,
                                    "update_mode": args.update_mode,
                                    "hot_sources": args.hot_sources, "epoch": args.epoch,
                                    "budget_bytes": budget, "trace_sha256": "NA",
                                    "updates": "NA", "increase_count": "NA",
                                    "decrease_count": "NA", "source_cap": "NA",
                                    "accounted_algorithm_state_bytes": "NA",
                                    "peak_accounted_algorithm_state_bytes": "NA",
                                    "accounting_version": "NA",
                                    **{f: "NA" for f in TELEMETRY_FIELDS},
                                    "cold_queries": "NA", "resident_queries": "NA",
                                    "promotions": "NA", "evictions": "NA",
                                    "rebuilds": "NA", "repair_aborts": "NA",
                                    "peak_child_rss_kb": (time_file.read_text().strip()
                                                          if time_file.is_file() else "NA"),
                                    "elapsed_wall_s": f"{wall:.6f}", "start_utc": started,
                                    "exit_code": rc, "claim_ids": "C1;C2;C3",
                                }

                                lines = stdout.strip().splitlines()
                                parsed = None
                                if rc == 0 and len(lines) == 1:
                                    try:
                                        parsed = parse_phase_line(lines[0])
                                        if parsed["baseline"] != baseline or parsed["schema_version"] != "2":
                                            raise ValueError("unexpected phase row")
                                        validate_v2_reconciliation(parsed)
                                    except (ValueError, KeyError):
                                        parsed = None
                                        rc = 98
                                elif rc == 0:
                                    rc = 98

                                if parsed is not None:
                                    row.update(
                                        trace_sha256=parsed["trace_sha256"],
                                        updates=parsed["updates"],
                                        increase_count=parsed["increase_count"],
                                        decrease_count=parsed["decrease_count"],
                                        source_cap=parsed["cap"],
                                        accounted_algorithm_state_bytes=parsed["accounted_algorithm_state_bytes"],
                                        peak_accounted_algorithm_state_bytes=parsed["peak_accounted_algorithm_state_bytes"],
                                        accounting_version=parsed["accounting_version"],
                                        cold_queries=parsed["cold_queries"],
                                        resident_queries=parsed["resident_queries"],
                                        promotions=parsed["promotions"],
                                        evictions=parsed["evictions"],
                                        rebuilds=parsed["rebuilds"],
                                        repair_aborts=parsed["repair_aborts"],
                                    )
                                    row.update({f: parsed[f] for f in TELEMETRY_FIELDS})
                                    if int(parsed["persistent_state_budget_bytes"]) != budget:
                                        rc = 96
                                    if int(parsed["accounted_algorithm_state_bytes"]) > budget:
                                        rc = 96
                                    if int(parsed["peak_accounted_algorithm_state_bytes"]) > budget:
                                        rc = 96
                                    signature = (
                                        parsed["trace_sha256"], int(parsed["queries"]),
                                        int(parsed["updates"]), int(parsed["increase_count"]),
                                        int(parsed["decrease_count"]),
                                    )
                                    if expected_signature is None:
                                        expected_signature = signature
                                    elif signature != expected_signature:
                                        rc = 97
                                    if expected_accounting_version is None:
                                        expected_accounting_version = parsed["accounting_version"]
                                    elif parsed["accounting_version"] != expected_accounting_version:
                                        rc = 96

                                row["exit_code"] = rc
                                writer.writerow(row)
                                stream.flush()
                                print(
                                    f"{key} budget={budget} exit={rc} "
                                    f"accounted={row['accounted_algorithm_state_bytes']} "
                                    f"peak={row['peak_accounted_algorithm_state_bytes']} "
                                    f"sha={row['trace_sha256']} schema={row['schema_version']}",
                                    flush=True,
                                )
                                if rc:
                                    failed = True
                                    break
                            if failed:
                                break
                        if failed:
                            break
                    if failed:
                        break
                if failed:
                    break
            if failed:
                break

    summary = {
        "schema": "ades-pr44-equal-byte-v2",
        "phase_schema_version": 2,
        "claim_ids": ["C1", "C2", "C3"],
        "status": "PASS" if not failed else "FAIL",
        "cells_attempted": cell,
        "rule": "same budget + accounting version + trace identity; peak accounted bytes <= budget; telemetry reconciles",
    }
    (out / "SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
