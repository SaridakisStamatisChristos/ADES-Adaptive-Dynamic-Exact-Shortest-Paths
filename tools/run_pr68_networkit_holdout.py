#!/usr/bin/env python3
import argparse
import csv
import json
import math
import pathlib
import statistics
import subprocess
import sys

FROZEN_ADES = "cf7cbc3ed7d9d37ad673a540485a1eee781d47fe"
NETWORKIT_COMMIT = "6b3878ee00a6eadd50be3b3047fbcae56bcac685"
BUDGET_BYTES = 67_108_864
PAIR_REPETITIONS = 2
EXPECTED_FAMILIES = ["uniform", "hot-pool-2", "hot-pool-4", "rotating-hot-4"]
EXPECTED_CELL_COUNT = 24

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
    p.add_argument("--manifest", required=True)
    p.add_argument("--oracle-builder", required=True)
    p.add_argument("--ades", required=True)
    p.add_argument("--networkit", required=True)
    p.add_argument("--pair-driver", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args()


def run(cmd, *, stdout_path=None):
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if stdout_path is not None:
        pathlib.Path(stdout_path).write_text(proc.stdout, encoding="utf-8")
        pathlib.Path(str(stdout_path) + ".stderr").write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(map(str, cmd))}\n"
            f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
    return proc


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write_json(path, data):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def gmean(values):
    if not values or any(v <= 0 for v in values):
        raise RuntimeError("geometric mean requires positive values")
    return math.exp(statistics.fmean(math.log(v) for v in values))


def log_ci(values):
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


def classify(stat):
    lo, hi = stat["ci95"]
    if lo is None or hi is None:
        return "INCONCLUSIVE"
    if lo > 1.0:
        return "ADES_FASTER"
    if hi < 1.0:
        return "NETWORKIT_FASTER"
    return "INCONCLUSIVE"


def cell_abs_times(pair_path):
    pairs = load_json(pair_path)
    out = {}
    for impl in ("networkit", "ades"):
        for metric in ("algorithm_ns", "query_ns", "update_ns"):
            vals = [row[f"{impl}_{metric}"] for row in pairs]
            out[f"{impl}_{metric}_geomean"] = gmean(vals)
    return out


def group_summary(cells):
    return {
        "total": log_ci([c["speedup_networkit_over_ades"] for c in cells]),
        "query_only": log_ci([c["query_speedup_networkit_over_ades"] for c in cells]),
        "update_only": log_ci([c["update_speedup_networkit_over_ades"] for c in cells]),
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_networkit_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_networkit_over_ades"] < 1.0],
        "networkit_cache_hits": sum(c["networkit_cache_hits"] for c in cells),
        "networkit_cache_misses": sum(c["networkit_cache_misses"] for c in cells),
        "networkit_cache_evictions": sum(c["networkit_cache_evictions"] for c in cells),
        "mean_source_reuse_fraction": statistics.fmean(c["source_reuse_fraction"] for c in cells),
    }


def main():
    a = parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    manifest = load_json(a.manifest)
    cells_cfg = manifest["cells"]
    if len(cells_cfg) != EXPECTED_CELL_COUNT:
        raise RuntimeError(f"expected {EXPECTED_CELL_COUNT} preregistered cells, got {len(cells_cfg)}")
    families = sorted({c["family"]["name"] for c in cells_cfg})
    if families != sorted(EXPECTED_FAMILIES):
        raise RuntimeError(f"source-family identity mismatch: {families}")

    prereg = {
        "schema": "ades-pr68-networkit-preregistered-holdout-v1",
        "status": "PREREGISTERED_BY_PR68_COMMIT_BEFORE_PERFORMANCE_EXECUTION",
        "frozen_ades_commit": FROZEN_ADES,
        "networkit_version": "11.2.2",
        "networkit_commit": NETWORKIT_COMMIT,
        "topology_family": manifest["design"]["topology_family"],
        "graph_seeds": manifest["design"]["graph_seeds"],
        "workload_seeds": manifest["design"]["workload_seeds"],
        "source_families": EXPECTED_FAMILIES,
        "cell_count": EXPECTED_CELL_COUNT,
        "pair_repetitions": PAIR_REPETITIONS,
        "persistent_state_budget_bytes": BUDGET_BYTES,
        "primary_ratio": "NetworKit algorithm_ns / ADES algorithm_ns; >1 favors ADES",
        "sensitivity_ratio": "NetworKit query_ns / ADES query_ns; >1 favors ADES",
        "primary_inference_unit": "one graph-seed/source-family/workload-seed cell after order-balanced within-cell pairing",
        "strong_pass_rule": "exactness all 24 cells; aggregate total and query-only lower 95% cell-level log-ratio CI >1; and every preregistered source family total and query-only lower 95% CI >1",
        "update_interpretation": "record update-only ratio but do not promote it as representation-neutral: one logical undirected update is two ADES directed-arc updates versus NetworKit remove+add on one undirected edge",
        "claim_boundary": "A strong pass supports only practical superiority over pinned NetworKit DynDijkstra on this preregistered non-road preferential-attachment slice, when combined with prior separately preserved road/Velo evidence; it is not universal or theoretical SOTA.",
    }
    write_json(out / "preregistered_design.json", prereg)

    cells = []
    for cfg in cells_cfg:
        cell_id = cfg["cell_id"]
        cell_dir = out / "cells" / cell_id
        cell_dir.mkdir(parents=True, exist_ok=True)
        graph = pathlib.Path(cfg["graph_path"])
        trace = pathlib.Path(cfg["trace_path"])
        oracle = cell_dir / "oracle.txt"

        if cfg["family"]["kind"] != "uniform" and cfg["source_reuse_fraction"] < 0.80:
            raise RuntimeError(f"{cell_id}: source reuse below preregistered threshold")
        if cfg["family"]["kind"] == "uniform" and cfg["source_reuse_fraction"] != 0.0:
            raise RuntimeError(f"{cell_id}: uniform source-reuse control violated")
        if cfg["logical_increase_count"] <= 0 or cfg["logical_decrease_count"] <= 0:
            raise RuntimeError(f"{cell_id}: both logical update directions required")
        if cfg["raw_symmetric_arc_update_count"] != 2 * cfg["logical_update_count"]:
            raise RuntimeError(f"{cell_id}: symmetric update-count contract violated")

        run([a.oracle_builder, str(graph), str(trace), str(oracle)],
            stdout_path=cell_dir / "oracle.stdout.txt")
        oracle_stdout = (cell_dir / "oracle.stdout.txt").read_text(encoding="utf-8")
        if "agreement=PASS" not in oracle_stdout:
            raise RuntimeError(f"{cell_id}: independent Boost/ADES oracle agreement missing")

        matched = cell_dir / "matched"
        run([
            sys.executable, a.pair_driver,
            "--graph", str(graph),
            "--trace", str(trace),
            "--oracle", str(oracle),
            "--ades", a.ades,
            "--networkit", a.networkit,
            "--out", str(matched),
            "--repetitions", str(PAIR_REPETITIONS),
            "--budget-bytes", str(BUDGET_BYTES),
            "--candidate-commit", FROZEN_ADES,
            "--expected-graph-sha", cfg["graph_sha256"],
            "--expected-trace-sha", cfg["trace_sha256"],
        ], stdout_path=cell_dir / "matched.stdout.txt")

        summary = load_json(matched / "summary.json")
        abs_times = cell_abs_times(matched / "pairs.json")
        cells.append({
            "cell_id": cell_id,
            "graph_seed": cfg["graph_seed"],
            "workload_seed": cfg["workload_seed"],
            "family": cfg["family"]["name"],
            "graph_sha256": cfg["graph_sha256"],
            "trace_sha256": cfg["trace_sha256"],
            "config_sha256": cfg["config_sha256"],
            "query_count": cfg["query_count"],
            "logical_update_count": cfg["logical_update_count"],
            "raw_update_count": cfg["raw_symmetric_arc_update_count"],
            "logical_increase_count": cfg["logical_increase_count"],
            "logical_decrease_count": cfg["logical_decrease_count"],
            "unique_query_sources": cfg["unique_query_sources"],
            "source_reuse_fraction": cfg["source_reuse_fraction"],
            "exactness": summary["exactness"],
            "speedup_networkit_over_ades": summary["geomean_networkit_over_ades"],
            "query_speedup_networkit_over_ades": summary["query_geomean_networkit_over_ades"],
            "update_speedup_networkit_over_ades": summary["update_geomean_networkit_over_ades"],
            "within_cell_total_ci95": summary["ci95_networkit_over_ades"],
            "within_cell_query_ci95": summary["query_ci95_networkit_over_ades"],
            "networkit_cache_hits": summary["networkit_cache_hits"],
            "networkit_cache_misses": summary["networkit_cache_misses"],
            "networkit_cache_evictions": summary["networkit_cache_evictions"],
            "networkit_peak_accounted_state_bytes": summary["networkit_peak_accounted_state_bytes_max"],
            "ades_peak_accounted_state_bytes": summary["ades_peak_accounted_state_bytes_max"],
            "networkit_peak_rss_kb": summary["networkit_peak_rss_kb_max"],
            "ades_peak_rss_kb": summary["ades_peak_rss_kb_max"],
            "networkit_per_retained_source_accounted_bytes": summary["networkit_per_retained_source_accounted_bytes"],
            **abs_times,
        })

    total = log_ci([c["speedup_networkit_over_ades"] for c in cells])
    query = log_ci([c["query_speedup_networkit_over_ades"] for c in cells])
    update = log_ci([c["update_speedup_networkit_over_ades"] for c in cells])

    groups = {"by_family": {}, "by_graph_seed": {}}
    for family in EXPECTED_FAMILIES:
        groups["by_family"][family] = group_summary([c for c in cells if c["family"] == family])
    for graph_seed in sorted({c["graph_seed"] for c in cells}):
        groups["by_graph_seed"][str(graph_seed)] = group_summary([c for c in cells if c["graph_seed"] == graph_seed])

    exact = all(c["exactness"] == "PASS" for c in cells)
    family_pass = {
        family: classify(stat["total"]) == "ADES_FASTER" and
                classify(stat["query_only"]) == "ADES_FASTER"
        for family, stat in groups["by_family"].items()
    }
    strong_pass = (
        exact and classify(total) == "ADES_FASTER" and classify(query) == "ADES_FASTER"
        and all(family_pass.values())
    )

    result = {
        "schema": "ades-pr68-networkit-nonroad-holdout-result-v1",
        "frozen_ades_commit": FROZEN_ADES,
        "networkit_commit": NETWORKIT_COMMIT,
        "cell_count": len(cells),
        "exactness_all_cells": exact,
        "strong_preregistered_pass": strong_pass,
        "total": total,
        "total_verdict": classify(total),
        "query_only": query,
        "query_only_verdict": classify(query),
        "update_only": update,
        "family_strong_pass": family_pass,
        "groups": groups,
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_networkit_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_networkit_over_ades"] < 1.0],
        "minimum_total_cell_speedup": min(c["speedup_networkit_over_ades"] for c in cells),
        "maximum_total_cell_speedup": max(c["speedup_networkit_over_ades"] for c in cells),
        "minimum_query_cell_speedup": min(c["query_speedup_networkit_over_ades"] for c in cells),
        "maximum_query_cell_speedup": max(c["query_speedup_networkit_over_ades"] for c in cells),
        "networkit_cache_hits_total": sum(c["networkit_cache_hits"] for c in cells),
        "networkit_cache_misses_total": sum(c["networkit_cache_misses"] for c in cells),
        "networkit_cache_evictions_total": sum(c["networkit_cache_evictions"] for c in cells),
        "verdict": (
            "HOLDOUT_SUPPORTS_NONROAD_NETWORKIT_SLICE_SUPERIORITY"
            if strong_pass else "HOLDOUT_DOES_NOT_SUPPORT_STRONG_NONROAD_SUPERIORITY"
        ),
        "claim_boundary": prereg["claim_boundary"],
        "update_interpretation": prereg["update_interpretation"],
        "cells": cells,
    }
    write_json(out / "summary.json", result)
    with (out / "cells.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(cells[0].keys()))
        w.writeheader(); w.writerows(cells)

    print(json.dumps({
        "exactness_all_cells": exact,
        "strong_preregistered_pass": strong_pass,
        "total": total,
        "query_only": query,
        "family_strong_pass": family_pass,
        "loss_cells_total": result["loss_cells_total"],
        "loss_cells_query_only": result["loss_cells_query_only"],
        "verdict": result["verdict"],
    }, indent=2, sort_keys=True))
    # Exactness failure is a hard CI failure.  A performance falsification is a
    # valid research outcome and therefore does not fail CI.
    return 0 if exact else 3


if __name__ == "__main__":
    raise SystemExit(main())
