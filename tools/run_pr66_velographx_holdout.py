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
FROZEN_PR65 = "b8ce372b957d53e7e9f72807a3cb4102a8b1697c"
FROZEN_VELO = "2aa84ce81a180487239c661b65639e5100136075"
SEEDS = [401, 503, 607]
FAMILIES = [
    {"name": "uniform", "generator": "uniform", "hot_sources": 4, "epoch_queries": 20, "locality_percent": 0},
    {"name": "hot-pool-2", "generator": "hot-pool", "hot_sources": 2, "epoch_queries": 20, "locality_percent": 0},
    {"name": "hot-pool-4", "generator": "hot-pool", "hot_sources": 4, "epoch_queries": 20, "locality_percent": 0},
    {"name": "rotating-hot-4", "generator": "rotating-hot", "hot_sources": 4, "epoch_queries": 20, "locality_percent": 0},
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
    p.add_argument("--velo", required=True)
    p.add_argument("--pair-driver", required=True)
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


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write_json(path, data):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
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
        return "ADES_FASTER"
    if hi < 1.0:
        return "VELO_FASTER"
    return "INCONCLUSIVE"


def trace_source_stats(path):
    sources = []
    with open(path, encoding="ascii") as f:
        for line in f:
            if line.startswith("QUERY "):
                parts = line.split()
                sources.append(int(parts[1]))
    unique = len(set(sources))
    return {
        "query_sources": len(sources),
        "unique_query_sources": unique,
        "source_reuse_queries": len(sources) - unique,
        "source_reuse_fraction": (len(sources) - unique) / len(sources) if sources else 0.0,
    }


def cell_abs_times(pair_path):
    pairs = load_json(pair_path)
    out = {}
    for impl in ("velo", "ades"):
        for metric in ("algorithm_ns", "query_ns", "update_ns"):
            values = [row[f"{impl}_{metric}"] for row in pairs]
            out[f"{impl}_{metric}_geomean"] = gmean(values)
    return out


def group_summary(cells):
    return {
        "total": log_ci([c["speedup_velo_over_ades"] for c in cells]),
        "query_only": log_ci([c["query_speedup_velo_over_ades"] for c in cells]),
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_velo_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_velo_over_ades"] < 1.0],
        "velo_cache_hits": sum(c["velo_cache_hits"] for c in cells),
        "velo_cache_misses": sum(c["velo_cache_misses"] for c in cells),
        "velo_cache_evictions": sum(c["velo_cache_evictions"] for c in cells),
        "mean_source_reuse_fraction": statistics.fmean(c["source_reuse_fraction"] for c in cells),
    }


def main():
    a = parse_args()
    candidate = pathlib.Path(a.candidate_root)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    design = {
        "schema": "ades-pr66-velographx-preregistered-holdout-v1",
        "status": "PREREGISTERED_BEFORE_EXECUTION",
        "frozen_ades_commit": FROZEN_ADES,
        "frozen_pr65_adapter_commit": FROZEN_PR65,
        "frozen_velographx_commit": FROZEN_VELO,
        "graphs": GRAPHS,
        "seeds": SEEDS,
        "source_families": FAMILIES,
        "query_count": QUERY_COUNT,
        "update_interval": UPDATE_INTERVAL,
        "update_mode": UPDATE_MODE,
        "magnitude": MAGNITUDE,
        "pair_repetitions": PAIR_REPETITIONS,
        "persistent_state_budget_bytes": BUDGET_BYTES,
        "primary_ratio": "VeloGraphX algorithm_ns / ADES algorithm_ns",
        "sensitivity_ratio": "VeloGraphX query_ns / ADES query_ns",
        "primary_inference_unit": "one graph/family/seed workload cell after order-balanced within-cell pairing",
        "strong_pass_rule": "exactness in all cells; overall total and query-only lower 95% CI > 1; and every preregistered source-family total and query-only lower 95% CI > 1",
        "claim_boundary": "If the strong pass rule survives, support is limited to practical superiority over this pinned adapter-mediated VeloGraphX dynamic-maintenance comparator on the frozen holdout slice; no theoretical-SOTA claim and no universal dynamic-SOTA claim.",
    }
    write_json(out / "preregistered_design.json", design)

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
                    str(QUERY_COUNT), family["generator"], str(UPDATE_INTERVAL),
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

                source_stats = trace_source_stats(trace)
                if family["name"] in ("hot-pool-2", "hot-pool-4") and source_stats["source_reuse_fraction"] < 0.80:
                    raise RuntimeError(f"{cell_id}: intended source-reuse family has insufficient reuse")

                run([
                    a.oracle_builder, str(graph_path), str(trace), str(oracle)
                ], stdout_path=cell_dir / "oracle.stdout.txt")
                if "agreement=PASS" not in (cell_dir / "oracle.stdout.txt").read_text(encoding="utf-8"):
                    raise RuntimeError(f"{cell_id}: independent oracle agreement missing")

                matched_dir = cell_dir / "matched"
                run([
                    sys.executable, a.pair_driver,
                    "--graph", str(graph_path),
                    "--trace", str(trace),
                    "--oracle", str(oracle),
                    "--ades", a.ades,
                    "--velo", a.velo,
                    "--out", str(matched_dir),
                    "--repetitions", str(PAIR_REPETITIONS),
                    "--budget-bytes", str(BUDGET_BYTES),
                    "--candidate-commit", FROZEN_ADES,
                    "--expected-graph-sha", graph_cfg["sha256"],
                    "--expected-trace-sha", meta["trace_sha256"],
                ], stdout_path=cell_dir / "matched.stdout.txt")

                summary = load_json(matched_dir / "summary.json")
                abs_times = cell_abs_times(matched_dir / "pairs.json")
                cells.append({
                    "cell_id": cell_id,
                    "graph": graph_cfg["name"],
                    "family": family["name"],
                    "generator_family": family["generator"],
                    "hot_sources": family["hot_sources"],
                    "seed": seed,
                    "config_sha256": meta["config_sha256"],
                    "trace_sha256": meta["trace_sha256"],
                    "query_count": meta["query_count"],
                    "update_count": meta["update_count"],
                    "increase_count": meta["increase_count"],
                    "decrease_count": meta["decrease_count"],
                    "exactness": summary["exactness"],
                    **source_stats,
                    "speedup_velo_over_ades": summary["geomean_velo_over_ades"],
                    "query_speedup_velo_over_ades": summary["query_geomean_velo_over_ades"],
                    "update_speedup_velo_over_ades": summary["update_geomean_velo_over_ades"],
                    "within_cell_ci95": summary["ci95_velo_over_ades"],
                    "velo_cache_hits": summary["velo_cache_hits"],
                    "velo_cache_misses": summary["velo_cache_misses"],
                    "velo_cache_evictions": summary["velo_cache_evictions"],
                    "velo_peak_accounted_state_bytes": summary["velo_peak_accounted_state_bytes_max"],
                    "ades_peak_accounted_state_bytes": summary["ades_peak_accounted_state_bytes_max"],
                    "velo_peak_rss_kb": summary["velo_peak_rss_kb_max"],
                    "ades_peak_rss_kb": summary["ades_peak_rss_kb_max"],
                    **abs_times,
                })

    total = log_ci([c["speedup_velo_over_ades"] for c in cells])
    query = log_ci([c["query_speedup_velo_over_ades"] for c in cells])
    update = log_ci([c["update_speedup_velo_over_ades"] for c in cells])

    groups = {"by_graph": {}, "by_family": {}}
    for graph in sorted({c["graph"] for c in cells}):
        groups["by_graph"][graph] = group_summary([c for c in cells if c["graph"] == graph])
    for family in [f["name"] for f in FAMILIES]:
        groups["by_family"][family] = group_summary([c for c in cells if c["family"] == family])

    exact = all(c["exactness"] == "PASS" for c in cells)
    family_pass = {}
    for family, stat in groups["by_family"].items():
        family_pass[family] = (
            classify(stat["total"]) == "ADES_FASTER" and
            classify(stat["query_only"]) == "ADES_FASTER"
        )
    strong_pass = (
        exact and classify(total) == "ADES_FASTER" and classify(query) == "ADES_FASTER"
        and all(family_pass.values())
    )

    result = {
        "schema": "ades-pr66-velographx-fresh-holdout-result-v1",
        "frozen_ades_commit": FROZEN_ADES,
        "frozen_pr65_adapter_commit": FROZEN_PR65,
        "frozen_velographx_commit": FROZEN_VELO,
        "cell_count": len(cells),
        "exactness_all_cells": exact,
        "total_speedup": total,
        "total_verdict": classify(total),
        "query_only_speedup": query,
        "query_only_verdict": classify(query),
        "update_speedup": update,
        "loss_cells_total": [c["cell_id"] for c in cells if c["speedup_velo_over_ades"] < 1.0],
        "loss_cells_query_only": [c["cell_id"] for c in cells if c["query_speedup_velo_over_ades"] < 1.0],
        "minimum_cell_speedup": min(c["speedup_velo_over_ades"] for c in cells),
        "maximum_cell_speedup": max(c["speedup_velo_over_ades"] for c in cells),
        "minimum_query_only_speedup": min(c["query_speedup_velo_over_ades"] for c in cells),
        "maximum_query_only_speedup": max(c["query_speedup_velo_over_ades"] for c in cells),
        "family_pass": family_pass,
        "groups": groups,
        "velo_cache_hits_total": sum(c["velo_cache_hits"] for c in cells),
        "velo_cache_misses_total": sum(c["velo_cache_misses"] for c in cells),
        "velo_cache_evictions_total": sum(c["velo_cache_evictions"] for c in cells),
        "strong_preregistered_pass": strong_pass,
        "verdict": "HOLDOUT_SUPPORTS_PRACTICAL_DYNAMIC_SLICE_SUPERIORITY" if strong_pass else ("FAIL_EXACTNESS" if not exact else "HOLDOUT_INCONCLUSIVE_OR_MIXED"),
        "claim_boundary": "A strong pass supports a narrow practical superiority claim over the pinned, adapter-qualified VeloGraphX dynamic-maintenance comparator on this preregistered NY source-reuse/churn slice under the frozen online exactness, timing and 64 MiB logical-state contract. It does not establish theoretical SOTA or universal dynamic SOTA.",
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
        print(f"PR66 VeloGraphX holdout failed: {exc}", file=sys.stderr)
        raise
