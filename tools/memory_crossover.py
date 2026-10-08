#!/usr/bin/env python3
"""Matched-trace bounded-residency crossover diagnostic. Stdlib only."""
import argparse
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

BASELINES = ("B2L", "B3L", "B4")
PHASE_FIELDS = (
    "baseline", "family", "seed", "trace_sha256", "queries", "updates",
    "increase_count", "decrease_count", "update_every", "hot_sources", "epoch",
    "cap", "algorithm_ns", "cold_queries", "resident_queries", "promotions",
    "evictions", "rebuilds", "repair_aborts",
)
TRACE_RE = re.compile(
    r"TRACE_IDENTITY sha256=([0-9a-f]{64}) queries=(\d+) updates=(\d+) "
    r"increases=(\d+) decreases=(\d+) unchanged=(\d+) mode=(\w+)"
)


def positive(value):
    n = int(value)
    if n <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return n


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", default="./build/ades_phase")
    p.add_argument("--graph", required=True)
    p.add_argument("--output", default="results/memory-crossover")
    p.add_argument("--queries", type=positive, default=10)
    p.add_argument("--repeats", type=positive, default=2)
    p.add_argument("--caps", type=positive, nargs="+", default=[2, 8])
    p.add_argument("--seeds", type=int, nargs="+", default=[7, 17])
    p.add_argument("--families", nargs="+", choices=["uniform", "zipf", "single-hot",
        "rotating-hot", "hot-pool", "churn"], default=["uniform", "single-hot"])
    p.add_argument("--update-every", type=int, nargs="+", default=[0, 10])
    p.add_argument("--update-mode", choices=["random", "alternating"], default="random")
    p.add_argument("--require-both-update-directions", action="store_true")
    p.add_argument("--hot-sources", type=positive, default=4)
    p.add_argument("--epoch", type=positive, default=250)
    p.add_argument("--timeout", type=positive, default=180)
    args = p.parse_args()
    if any(x < 0 for x in args.update_every):
        p.error("update-every must be nonnegative")
    if args.require_both_update_directions and any(x == 0 for x in args.update_every):
        p.error("--require-both-update-directions cannot be used with update-every=0")

    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)
    binary = Path(args.binary).resolve()
    graph = Path(args.graph).resolve()
    if not binary.is_file() or not graph.is_file():
        p.error("binary and graph must exist")

    graph_sha = hashlib.sha256(graph.read_bytes()).hexdigest()
    meta = dict(
        utc_started=dt.datetime.now(dt.timezone.utc).isoformat(),
        git_commit=os.environ.get("GITHUB_SHA", "local-unpinned"),
        graph_sha256=graph_sha,
        graph=str(graph),
        binary=str(binary),
        platform=platform.platform(),
        python=sys.version,
        runner=os.environ.get("RUNNER_NAME", "local"),
        config={k: v for k, v in vars(args).items() if k not in ("binary", "graph", "output")},
        comparison_class="matched-source-cap; NOT equal-RSS",
        evidence_class="diagnostic; exact answers checked against independent Dijkstra oracle",
        trace_identity="canonical operation stream SHA-256",
    )
    (out / "manifest.json").write_text(json.dumps(meta, indent=2) + "\n")

    fields = [
        "cell", "repeat", "order", "baseline", "family", "seed", "queries",
        "update_every", "update_mode", "hot_sources", "epoch", "cap", "start_utc",
        "elapsed_wall_s", "peak_child_rss_kb", "exit_code", "trace_sha256", "updates",
        "increase_updates", "decrease_updates", "unchanged_updates", "algorithm_ns",
        "cold_queries", "resident_queries", "promotions", "evictions", "rebuilds",
        "repair_aborts",
    ]
    phase_stats = PHASE_FIELDS[12:]
    failed = False
    trace_signatures = {}

    with (out / "measurements.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        cell = 0
        for cap in args.caps:
            for seed in args.seeds:
                for family in args.families:
                    for ue in args.update_every:
                        cell += 1
                        for rep in range(args.repeats):
                            rotation = (rep + cell - 1) % len(BASELINES)
                            order = BASELINES[rotation:] + BASELINES[:rotation]
                            for idx, baseline in enumerate(order):
                                key = f"cell{cell:03d}-rep{rep:02d}-{baseline}"
                                cmd = [
                                    str(binary), str(graph), baseline, family,
                                    str(args.queries), str(ue), str(args.hot_sources),
                                    str(args.epoch), str(seed), str(cap), args.update_mode,
                                ]
                                started = dt.datetime.now(dt.timezone.utc).isoformat()
                                time_file = out / f"{key}.time"
                                t0 = time.monotonic()
                                try:
                                    proc = subprocess.run(
                                        ["/usr/bin/time", "-f", "%M", "-o", str(time_file), *cmd],
                                        capture_output=True, text=True, timeout=args.timeout, check=False,
                                    )
                                    rc, stdout, stderr = proc.returncode, proc.stdout, proc.stderr
                                except subprocess.TimeoutExpired as exc:
                                    rc = 124
                                    stdout = ((exc.stdout or b"").decode(errors="replace")
                                              if isinstance(exc.stdout, bytes) else (exc.stdout or ""))
                                    stderr = ((exc.stderr or b"").decode(errors="replace")
                                              if isinstance(exc.stderr, bytes) else (exc.stderr or ""))
                                    stderr += "\nTIMEOUT\n"

                                wall = time.monotonic() - t0
                                (out / f"{key}.stdout").write_text(stdout)
                                (out / f"{key}.stderr").write_text(stderr)
                                values = stdout.strip().splitlines()
                                row = dict(
                                    cell=cell, repeat=rep, order=idx, baseline=baseline,
                                    family=family, seed=seed, queries=args.queries,
                                    update_every=ue, update_mode=args.update_mode,
                                    hot_sources=args.hot_sources, epoch=args.epoch, cap=cap,
                                    start_utc=started, elapsed_wall_s=f"{wall:.6f}",
                                    peak_child_rss_kb=(time_file.read_text().strip()
                                                       if time_file.is_file() else "NA"),
                                    exit_code=rc, trace_sha256="NA", updates="NA",
                                    increase_updates="NA", decrease_updates="NA",
                                    unchanged_updates="NA",
                                    **{f: "NA" for f in phase_stats},
                                )

                                parts = None
                                if rc == 0 and len(values) == 1:
                                    candidate = values[0].split(",")
                                    if len(candidate) == len(PHASE_FIELDS) and candidate[0] == baseline:
                                        parts = candidate
                                        parsed = dict(zip(PHASE_FIELDS, candidate))
                                        row.update({f: parsed[f] for f in phase_stats})
                                        row["trace_sha256"] = parsed["trace_sha256"]
                                        row["updates"] = parsed["updates"]
                                        row["increase_updates"] = parsed["increase_count"]
                                        row["decrease_updates"] = parsed["decrease_count"]
                                        row["unchanged_updates"] = str(
                                            int(parsed["updates"]) - int(parsed["increase_count"])
                                            - int(parsed["decrease_count"])
                                        )
                                    else:
                                        rc = 98
                                elif rc == 0:
                                    rc = 98

                                trace = TRACE_RE.search(stderr)
                                if rc == 0 and trace is None:
                                    rc = 97
                                if trace is not None:
                                    sha, queries, total, inc, dec, unchanged, mode = trace.groups()
                                    signature = (
                                        sha, int(queries), int(total), int(inc), int(dec),
                                        int(unchanged), mode,
                                    )
                                    if row["trace_sha256"] == "NA":
                                        row.update(
                                            trace_sha256=sha,
                                            updates=total,
                                            increase_updates=inc,
                                            decrease_updates=dec,
                                            unchanged_updates=unchanged,
                                        )
                                    if rc == 0 and mode != args.update_mode:
                                        rc = 97
                                    if rc == 0 and parts is not None:
                                        parsed = dict(zip(PHASE_FIELDS, parts))
                                        if parsed["trace_sha256"] != sha:
                                            rc = 97
                                        if int(parsed["queries"]) != int(queries):
                                            rc = 97
                                        if int(parsed["updates"]) != int(total):
                                            rc = 97
                                        if int(parsed["increase_count"]) != int(inc):
                                            rc = 97
                                        if int(parsed["decrease_count"]) != int(dec):
                                            rc = 97
                                    expected = trace_signatures.setdefault(cell, signature)
                                    if rc == 0 and signature != expected:
                                        rc = 97
                                    if (rc == 0 and args.require_both_update_directions and
                                            (int(total) == 0 or int(inc) == 0 or int(dec) == 0)):
                                        rc = 97

                                row["exit_code"] = rc
                                writer.writerow(row)
                                stream.flush()
                                print(
                                    f"{key} exit={rc} wall={wall:.3f}s "
                                    f"sha={row['trace_sha256']} updates={row['updates']} "
                                    f"+{row['increase_updates']} -{row['decrease_updates']}",
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
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
