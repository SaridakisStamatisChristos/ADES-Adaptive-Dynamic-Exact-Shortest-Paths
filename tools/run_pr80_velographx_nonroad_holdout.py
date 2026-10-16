#!/usr/bin/env python3
"""Execute and summarize the preregistered PR80 VeloGraphX non-road holdout."""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys

FROZEN_ADES = "cf7cbc3ed7d9d37ad673a540485a1eee781d47fe"
FROZEN_VELO = "2aa84ce81a180487239c661b65639e5100136075"
BUDGET_BYTES = 67_108_864
PAIR_REPETITIONS = 2
EXPECTED_GRAPH_SEEDS = [8209, 9103, 10037]
EXPECTED_WORKLOAD_SEEDS = [12007, 13001]
EXPECTED_FAMILIES = ["uniform", "hot-pool-2", "hot-pool-4", "rotating-hot-4"]

T95 = {
    1: 12.706204736, 2: 4.302652730, 3: 3.182446305,
    4: 2.776445105, 5: 2.570581836, 6: 2.446911851,
    7: 2.364624252, 8: 2.306004135, 9: 2.262157163,
    10: 2.228138852, 11: 2.200985160, 12: 2.178812830,
    13: 2.160368656, 14: 2.144786688, 15: 2.131449546,
    16: 2.119905299, 17: 2.109815578, 18: 2.100922040,
    19: 2.093024054, 20: 2.085963447, 21: 2.079613845,
    22: 2.073873068, 23: 2.068657610, 24: 2.063898562,
    25: 2.059538553, 26: 2.055529439, 27: 2.051830516,
    28: 2.048407142, 29: 2.045229642, 30: 2.042272456,
}


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--generated-root", required=True)
    p.add_argument("--oracle-builder", required=True)
    p.add_argument("--ades", required=True)
    p.add_argument("--velo", required=True)
    p.add_argument("--pair-driver", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args()


def load_json(path: Path):
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, obj) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run(cmd: list[str], *, stdout_path: Path) -> None:
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(cmd, text=True, capture_output=True)
    stdout_path.write_text(proc.stdout, encoding="utf-8")
    Path(str(stdout_path) + ".stderr").write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )


def gmean(values: list[float]) -> float:
    if not values or any(v <= 0 for v in values):
        raise RuntimeError("geometric mean requires positive values")
    return math.exp(statistics.fmean(math.log(v) for v in values))


def log_ci(values: list[float]) -> dict:
    logs = [math.log(v) for v in values]
    mean = statistics.fmean(logs)
    gm = math.exp(mean)
    if len(logs) < 2:
        return {"n": len(logs), "geomean": gm, "ci95": [None, None]}
    sd = statistics.stdev(logs)
    df = len(logs) - 1
    t = T95.get(df, 1.959963985)
    half = t * sd / math.sqrt(len(logs))
    return {"n": len(logs), "geomean": gm,
            "ci95": [math.exp(mean - half), math.exp(mean + half)]}


def lower_gt_one(stat: dict) -> bool:
    lo = stat["ci95"][0]
    return lo is not None and lo > 1.0


def group_summary(cells: list[dict]) -> dict:
    return {
        "total": log_ci([c["speedup_velo_over_ades"] for c in cells]),
        "query_only": log_ci([c["query_speedup_velo_over_ades"] for c in cells]),
        "update": log_ci([c["update_speedup_velo_over_ades"] for c in cells]),
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_velo_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_velo_over_ades"] < 1.0],
        "velo_cache_hits": sum(c["velo_cache_hits"] for c in cells),
        "velo_cache_misses": sum(c["velo_cache_misses"] for c in cells),
        "velo_cache_evictions": sum(c["velo_cache_evictions"] for c in cells),
        "mean_source_reuse_fraction": statistics.fmean(c["source_reuse_fraction"] for c in cells),
    }


def cell_abs_times(pair_path: Path) -> dict:
    pairs = load_json(pair_path)
    out = {}
    for impl in ("velo", "ades"):
        for metric in ("algorithm_ns", "query_ns", "update_ns"):
            out[f"{impl}_{metric}_geomean"] = gmean([row[f"{impl}_{metric}"] for row in pairs])
    return out


def main() -> int:
    a = parse_args()
    generated = Path(a.generated_root).resolve()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest = load_json(generated / "manifest.json")
    design = manifest["design"]

    if manifest.get("schema") != "ades-pr80-generated-manifest-v1":
        raise RuntimeError("unexpected PR80 manifest schema")
    if manifest.get("cell_count") != 24:
        raise RuntimeError("PR80 must contain exactly 24 cells")
    if design.get("graph_seeds") != EXPECTED_GRAPH_SEEDS:
        raise RuntimeError("graph seed drift")
    if design.get("workload_seeds") != EXPECTED_WORKLOAD_SEEDS:
        raise RuntimeError("workload seed drift")
    if [f["name"] for f in design.get("source_families", [])] != EXPECTED_FAMILIES:
        raise RuntimeError("source family drift")
    if design.get("vertices") != 15000 or design.get("attachments_per_new_vertex") != 5:
        raise RuntimeError("graph family parameter drift")
    if design.get("query_count") != 60 or design.get("update_interval_queries") != 5:
        raise RuntimeError("workload-size drift")

    prereg = {
        "schema": "ades-pr80-velographx-nonroad-holdout-design-v1",
        "status": "PREREGISTERED_BEFORE_TIMING",
        "contract": "docs/PR80_VELOGRAPHX_NONROAD_HOLDOUT_CONTRACT.md",
        "frozen_ades_commit": FROZEN_ADES,
        "frozen_velographx_commit": FROZEN_VELO,
        "persistent_state_budget_bytes": BUDGET_BYTES,
        "pair_repetitions": PAIR_REPETITIONS,
        "graph_seeds": EXPECTED_GRAPH_SEEDS,
        "workload_seeds": EXPECTED_WORKLOAD_SEEDS,
        "source_families": EXPECTED_FAMILIES,
        "cell_count": 24,
        "primary_ratio": "VeloGraphX algorithm_ns / ADES algorithm_ns",
        "sensitivity_ratio": "VeloGraphX query_ns / ADES query_ns",
        "strong_support_rule": "24/24 exact+oracle+integrity; overall, every source family, and every graph-instance total/query lower 95% CI > 1; zero total/query loss cells",
    }
    write_json(out / "preregistered_design.json", prereg)

    cells: list[dict] = []
    for meta in manifest["cells"]:
        cell_id = meta["cell_id"]
        graph = Path(meta["graph_path"])
        trace = Path(meta["trace_path"])
        if not graph.is_absolute():
            graph = generated / graph.relative_to(Path(a.generated_root)) if str(graph).startswith(str(Path(a.generated_root))) else graph.resolve()
        if not trace.is_absolute():
            trace = generated / trace.relative_to(Path(a.generated_root)) if str(trace).startswith(str(Path(a.generated_root))) else trace.resolve()
        # Generator normally records paths rooted at --generated-root. Resolve by basename/cell if cwd differs.
        if not graph.exists():
            graph = generated / "graphs" / Path(meta["graph_path"]).name
        if not trace.exists():
            trace = generated / "cells" / cell_id / "trace.txt"
        if not graph.exists() or not trace.exists():
            raise RuntimeError(f"{cell_id}: generated graph/trace missing")

        family = meta["family"]["name"]
        if family not in EXPECTED_FAMILIES:
            raise RuntimeError(f"{cell_id}: unexpected family")
        reuse = float(meta["source_reuse_fraction"])
        if family == "uniform" and reuse != 0.0:
            raise RuntimeError(f"{cell_id}: uniform reuse must be zero")
        if family != "uniform" and reuse < 0.80:
            raise RuntimeError(f"{cell_id}: source reuse below preregistered threshold")
        if meta["logical_increase_count"] <= 0 or meta["logical_decrease_count"] <= 0:
            raise RuntimeError(f"{cell_id}: missing update direction")

        cell_dir = out / "cells" / cell_id
        oracle = cell_dir / "oracle.txt"
        run([a.oracle_builder, str(graph), str(trace), str(oracle)], stdout_path=cell_dir / "oracle.stdout.txt")
        if "agreement=PASS" not in (cell_dir / "oracle.stdout.txt").read_text(encoding="utf-8"):
            raise RuntimeError(f"{cell_id}: independent oracle agreement missing")

        matched = cell_dir / "matched"
        run([
            sys.executable, a.pair_driver,
            "--graph", str(graph),
            "--trace", str(trace),
            "--oracle", str(oracle),
            "--ades", a.ades,
            "--velo", a.velo,
            "--out", str(matched),
            "--repetitions", str(PAIR_REPETITIONS),
            "--budget-bytes", str(BUDGET_BYTES),
            "--candidate-commit", FROZEN_ADES,
            "--expected-graph-sha", meta["graph_sha256"],
            "--expected-trace-sha", meta["trace_sha256"],
        ], stdout_path=cell_dir / "matched.stdout.txt")
        summary = load_json(matched / "summary.json")
        if summary.get("exactness") != "PASS":
            raise RuntimeError(f"{cell_id}: exactness failed")
        cells.append({
            "cell_id": cell_id,
            "graph_seed": meta["graph_seed"],
            "workload_seed": meta["workload_seed"],
            "family": family,
            "graph_sha256": meta["graph_sha256"],
            "trace_sha256": meta["trace_sha256"],
            "config_sha256": meta["config_sha256"],
            "query_count": meta["query_count"],
            "logical_update_count": meta["logical_update_count"],
            "raw_symmetric_arc_update_count": meta["raw_symmetric_arc_update_count"],
            "unique_query_sources": meta["unique_query_sources"],
            "source_reuse_fraction": reuse,
            "exactness": summary["exactness"],
            "speedup_velo_over_ades": summary["geomean_velo_over_ades"],
            "query_speedup_velo_over_ades": summary["query_geomean_velo_over_ades"],
            "update_speedup_velo_over_ades": summary["update_geomean_velo_over_ades"],
            "within_cell_total_ci95": summary["ci95_velo_over_ades"],
            "within_cell_query_ci95": summary["query_ci95_velo_over_ades"],
            "velo_cache_hits": summary["velo_cache_hits"],
            "velo_cache_misses": summary["velo_cache_misses"],
            "velo_cache_evictions": summary["velo_cache_evictions"],
            "velo_peak_accounted_state_bytes": summary["velo_peak_accounted_state_bytes_max"],
            "ades_peak_accounted_state_bytes": summary["ades_peak_accounted_state_bytes_max"],
            "velo_peak_rss_kb": summary["velo_peak_rss_kb_max"],
            "ades_peak_rss_kb": summary["ades_peak_rss_kb_max"],
            **cell_abs_times(matched / "pairs.json"),
        })

    if len(cells) != 24:
        raise RuntimeError("executed cell count drift")

    aggregate = group_summary(cells)
    by_family = {family: group_summary([c for c in cells if c["family"] == family])
                 for family in EXPECTED_FAMILIES}
    by_graph = {str(seed): group_summary([c for c in cells if c["graph_seed"] == seed])
                for seed in EXPECTED_GRAPH_SEEDS}

    exact = all(c["exactness"] == "PASS" for c in cells)
    state_ok = all(c["velo_peak_accounted_state_bytes"] <= BUDGET_BYTES and
                   c["ades_peak_accounted_state_bytes"] <= BUDGET_BYTES for c in cells)
    family_ok = all(lower_gt_one(s["total"]) and lower_gt_one(s["query_only"]) for s in by_family.values())
    graph_ok = all(lower_gt_one(s["total"]) and lower_gt_one(s["query_only"]) for s in by_graph.values())
    zero_losses = not aggregate["loss_cells_total"] and not aggregate["loss_cells_query_only"]
    strong = (exact and state_ok and lower_gt_one(aggregate["total"]) and
              lower_gt_one(aggregate["query_only"]) and family_ok and graph_ok and zero_losses)

    disposition = (
        "SUPPORTS_ADES_OVER_VELOGRAPHX_ON_PR80_NONROAD_ARBITRARY_SOURCE_SLICE"
        if strong else
        "FALSIFIES_STRONG_PR80_VELOGRAPHX_NONROAD_HYPOTHESIS_ON_TESTED_SLICE"
    )
    result = {
        "schema": "ades-pr80-velographx-nonroad-holdout-result-v1",
        "disposition": disposition,
        "frozen_ades_commit": FROZEN_ADES,
        "frozen_velographx_commit": FROZEN_VELO,
        "persistent_state_budget_bytes": BUDGET_BYTES,
        "cell_count": len(cells),
        "exactness_all_cells": exact,
        "state_budget_all_cells": state_ok,
        "aggregate": aggregate,
        "by_family": by_family,
        "by_graph_seed": by_graph,
        "family_lower_bound_rule_pass": family_ok,
        "graph_lower_bound_rule_pass": graph_ok,
        "zero_total_and_query_loss_cells": zero_losses,
        "strong_preregistered_pass": strong,
        "minimum_total_cell_speedup": min(c["speedup_velo_over_ades"] for c in cells),
        "minimum_query_cell_speedup": min(c["query_speedup_velo_over_ades"] for c in cells),
        "maximum_total_cell_speedup": max(c["speedup_velo_over_ades"] for c in cells),
        "maximum_query_cell_speedup": max(c["query_speedup_velo_over_ades"] for c in cells),
        "claim_boundary": "A strong pass supports only the frozen practical exact-online 64 MiB arbitrary-source non-road slice against the pinned VeloGraphX adapter. It is not theoretical or universal SOTA evidence.",
    }
    write_json(out / "holdout_summary.json", result)
    write_json(out / "cells.json", cells)
    with (out / "cells.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(cells[0].keys()))
        w.writeheader()
        w.writerows(cells)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"PR80 VeloGraphX non-road holdout integrity failure: {exc}", file=sys.stderr)
        raise
