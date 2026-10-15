#!/usr/bin/env python3
import argparse
import csv
import json
import math
import pathlib
import statistics
import subprocess
import sys

FROZEN_CANDIDATE = "cf7cbc3ed7d9d37ad673a540485a1eee781d47fe"
SEEDS = [101, 211, 307]
FAMILIES = [
    {"name": "uniform", "hot_sources": 8, "epoch_queries": 20, "locality_percent": 0},
    {"name": "hot-pool", "hot_sources": 8, "epoch_queries": 20, "locality_percent": 0},
    {"name": "rotating-hot", "hot_sources": 8, "epoch_queries": 20, "locality_percent": 0},
    {"name": "churn", "hot_sources": 8, "epoch_queries": 20, "locality_percent": 0},
]
GRAPHS = [
    {
        "name": "USA-road-d.NY",
        "file": "USA-road-d.NY.gr.gz",
        "sha256": "7b2446c7ffe6179efbc42af6812448e8cb782f12968ca2bb1d50d979343056d4",
    },
    {
        "name": "USA-road-t.NY",
        "file": "USA-road-t.NY.gr.gz",
        "sha256": "de902573142dbf12c42b065d9a6a679e4794372eb91d2748ee78089720377156",
    },
]
QUERY_COUNT = 100
UPDATE_INTERVAL = 5
UPDATE_MODE = "strict-alternating"
MAGNITUDE = "medium"
PAIR_REPETITIONS = 2
BUDGET_BYTES = 67108864

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
    p.add_argument("--candidate-root", required=True)
    p.add_argument("--workload-generator", required=True)
    p.add_argument("--oracle-builder", required=True)
    p.add_argument("--ades", required=True)
    p.add_argument("--boost", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args()


def run(cmd, *, stdout_path=None):
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if stdout_path:
        pathlib.Path(stdout_path).write_text(proc.stdout, encoding="utf-8")
        pathlib.Path(str(stdout_path) + ".stderr").write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(map(str, cmd))}\n"
            f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
    return proc


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
    return {
        "n": len(logs),
        "geomean": gm,
        "ci95": [math.exp(mean - half), math.exp(mean + half)],
    }


def classify(stat):
    lo, hi = stat["ci95"]
    if lo is None or hi is None:
        return "INCONCLUSIVE"
    if lo > 1.0:
        return "IMPROVEMENT"
    if hi < 1.0:
        return "REGRESSION"
    return "INCONCLUSIVE"


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def cell_abs_times(pair_path):
    pairs = load_json(pair_path)
    out = {}
    for impl in ("boost", "ades"):
        for metric in ("algorithm_ns", "query_ns", "update_ns"):
            values = [row[f"{impl}_{metric}"] for row in pairs]
            out[f"{impl}_{metric}_geomean"] = gmean(values)
    return out


def main():
    a = parse_args()
    candidate = pathlib.Path(a.candidate_root)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    design = {
        "schema": "ades-pr62-preregistered-holdout-v1",
        "status": "PREREGISTERED_BEFORE_EXECUTION",
        "frozen_candidate_commit": FROZEN_CANDIDATE,
        "graphs": GRAPHS,
        "seeds": SEEDS,
        "source_families": FAMILIES,
        "query_count": QUERY_COUNT,
        "update_interval": UPDATE_INTERVAL,
        "update_mode": UPDATE_MODE,
        "magnitude": MAGNITUDE,
        "pair_repetitions": PAIR_REPETITIONS,
        "persistent_state_budget_bytes": BUDGET_BYTES,
        "primary_ratio": "Boost algorithm_ns / ADES algorithm_ns",
        "sensitivity_ratio": "Boost query_ns / ADES query_ns",
        "primary_inference_unit": "one frozen graph/family/seed workload cell after order-balanced within-cell pairing",
        "claim_boundary": "external exact recomputation-baseline superiority on this frozen holdout slice only; not broad dynamic-maintenance SOTA",
    }
    write_json(out / "preregistered_design.json", design)

    pair_driver = candidate / "tools" / "run_pr60_matched_head_to_head.py"
    cells = []
    for graph_cfg in GRAPHS:
        graph_path = candidate / "benchmarks" / "data" / graph_cfg["file"]
        for family in FAMILIES:
            for seed in SEEDS:
                cell_id = f"{graph_cfg['name']}__{family['name']}__seed-{seed}"
                cell_dir = out / "cells" / cell_id
                cell_dir.mkdir(parents=True, exist_ok=True)
                trace = cell_dir / "trace.txt"
                metadata = cell_dir / "workload.json"
                oracle = cell_dir / "oracle.txt"

                run([
                    a.workload_generator,
                    str(graph_path), str(trace), str(metadata), str(seed),
                    str(QUERY_COUNT), family["name"], str(UPDATE_INTERVAL),
                    UPDATE_MODE, MAGNITUDE, str(family["hot_sources"]),
                    str(family["epoch_queries"]), str(family["locality_percent"]),
                ], stdout_path=cell_dir / "workload.stdout.txt")
                meta = load_json(metadata)
                if meta["graph_sha256"] != graph_cfg["sha256"]:
                    raise RuntimeError(f"{cell_id}: graph SHA mismatch")
                if meta["query_count"] != QUERY_COUNT:
                    raise RuntimeError(f"{cell_id}: query-count mismatch")
                if meta["increase_count"] <= 0 or meta["decrease_count"] <= 0:
                    raise RuntimeError(f"{cell_id}: strict-alternating trace lacks both update directions")

                run([
                    a.oracle_builder, str(graph_path), str(trace), str(oracle)
                ], stdout_path=cell_dir / "oracle.stdout.txt")

                matched_dir = cell_dir / "matched"
                run([
                    sys.executable, str(pair_driver),
                    "--graph", str(graph_path),
                    "--trace", str(trace),
                    "--oracle", str(oracle),
                    "--ades", a.ades,
                    "--boost", a.boost,
                    "--out", str(matched_dir),
                    "--repetitions", str(PAIR_REPETITIONS),
                    "--budget-bytes", str(BUDGET_BYTES),
                    "--candidate-commit", FROZEN_CANDIDATE,
                    "--expected-graph-sha", graph_cfg["sha256"],
                    "--expected-trace-sha", meta["trace_sha256"],
                ], stdout_path=cell_dir / "matched.stdout.txt")
                summary = load_json(matched_dir / "summary.json")
                abs_times = cell_abs_times(matched_dir / "pairs.json")
                cells.append({
                    "cell_id": cell_id,
                    "graph": graph_cfg["name"],
                    "family": family["name"],
                    "seed": seed,
                    "config_sha256": meta["config_sha256"],
                    "trace_sha256": meta["trace_sha256"],
                    "query_count": meta["query_count"],
                    "update_count": meta["update_count"],
                    "increase_count": meta["increase_count"],
                    "decrease_count": meta["decrease_count"],
                    "exactness": summary["exactness"],
                    "speedup_boost_over_ades": summary["geomean_boost_over_ades"],
                    "query_speedup_boost_over_ades": summary["query_geomean_boost_over_ades"],
                    "update_speedup_boost_over_ades": summary["update_geomean_boost_over_ades"],
                    "within_cell_ci95": summary["ci95_boost_over_ades"],
                    "ades_peak_accounted_state_bytes": summary["ades_peak_accounted_state_bytes_max"],
                    "boost_peak_accounted_state_bytes": summary["boost_peak_accounted_state_bytes_max"],
                    "ades_peak_rss_kb": summary["ades_peak_rss_kb_max"],
                    "boost_peak_rss_kb": summary["boost_peak_rss_kb_max"],
                    **abs_times,
                })

    total_values = [c["speedup_boost_over_ades"] for c in cells]
    query_values = [c["query_speedup_boost_over_ades"] for c in cells]
    update_values = [c["update_speedup_boost_over_ades"] for c in cells]
    total_stat = log_ci(total_values)
    query_stat = log_ci(query_values)
    update_stat = log_ci(update_values)

    groups = {"by_graph": {}, "by_family": {}}
    for graph in sorted({c["graph"] for c in cells}):
        subset = [c for c in cells if c["graph"] == graph]
        groups["by_graph"][graph] = {
            "total": log_ci([c["speedup_boost_over_ades"] for c in subset]),
            "query_only": log_ci([c["query_speedup_boost_over_ades"] for c in subset]),
            "loss_cells": [c["cell_id"] for c in subset if c["speedup_boost_over_ades"] < 1.0],
        }
    for family in sorted({c["family"] for c in cells}):
        subset = [c for c in cells if c["family"] == family]
        groups["by_family"][family] = {
            "total": log_ci([c["speedup_boost_over_ades"] for c in subset]),
            "query_only": log_ci([c["query_speedup_boost_over_ades"] for c in subset]),
            "loss_cells": [c["cell_id"] for c in subset if c["speedup_boost_over_ades"] < 1.0],
        }

    exact = all(c["exactness"] == "PASS" for c in cells)
    total_verdict = classify(total_stat)
    query_verdict = classify(query_stat)
    if exact and total_verdict == "IMPROVEMENT" and query_verdict == "IMPROVEMENT":
        verdict = "HOLDOUT_SUPPORTS_EXTERNAL_RECOMPUTATION_BASELINE_SUPERIORITY"
    elif not exact:
        verdict = "FAIL_EXACTNESS"
    else:
        verdict = "HOLDOUT_INCONCLUSIVE_OR_MIXED"

    result = {
        "schema": "ades-pr62-fresh-holdout-result-v1",
        "frozen_candidate_commit": FROZEN_CANDIDATE,
        "cell_count": len(cells),
        "exactness_all_cells": exact,
        "total_speedup": total_stat,
        "total_verdict": total_verdict,
        "query_only_speedup": query_stat,
        "query_only_verdict": query_verdict,
        "update_speedup": update_stat,
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_boost_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_boost_over_ades"] < 1.0],
        "minimum_cell_speedup": min(total_values),
        "maximum_cell_speedup": max(total_values),
        "minimum_query_only_speedup": min(query_values),
        "maximum_query_only_speedup": max(query_values),
        "groups": groups,
        "verdict": verdict,
        "claim_boundary": "If favorable, this supports only superiority to the pinned Boost.Graph exact recomputation baseline over the preregistered holdout slice under the frozen timing/memory contract. It does not establish broad dynamic-maintenance SOTA or theoretical SOTA.",
        "update_component_caveat": "Boost's PR59 adapter resolves update edge IDs by scanning Boost.Graph edges; therefore update-only ratios are implementation-specific. Query-only holdout inference is reported separately as the primary sensitivity check.",
    }
    write_json(out / "holdout_summary.json", result)
    write_json(out / "cells.json", cells)
    with open(out / "cells.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(cells[0].keys()))
        w.writeheader()
        w.writerows(cells)

    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"PR62 fresh holdout failed: {exc}", file=sys.stderr)
        raise
