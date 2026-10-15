#!/usr/bin/env python3
import argparse
import csv
import json
import math
import os
import pathlib
import statistics
import subprocess

NETWORKIT_COMMIT = "6b3878ee00a6eadd50be3b3047fbcae56bcac685"
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
    p.add_argument("--graph", required=True)
    p.add_argument("--trace", required=True)
    p.add_argument("--oracle", required=True)
    p.add_argument("--ades", required=True)
    p.add_argument("--networkit", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--repetitions", type=int, default=2)
    p.add_argument("--budget-bytes", type=int, default=67108864)
    p.add_argument("--candidate-commit", required=True)
    p.add_argument("--expected-graph-sha", required=True)
    p.add_argument("--expected-trace-sha", required=True)
    return p.parse_args()


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def run_one(name, exe, common_args, rep, out_dir, env):
    report = out_dir / f"rep-{rep:02d}-{name}.json"
    rss = out_dir / f"rep-{rep:02d}-{name}-max-rss-kb.txt"
    stdout = out_dir / f"rep-{rep:02d}-{name}.stdout.txt"
    stderr = out_dir / f"rep-{rep:02d}-{name}.stderr.txt"
    cmd = ["/usr/bin/time", "-f", "%M", "-o", str(rss), exe, *common_args, str(report)]
    proc = subprocess.run(cmd, env=env, text=True, capture_output=True)
    stdout.write_text(proc.stdout, encoding="utf-8")
    stderr.write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"{name} repetition {rep} failed with exit {proc.returncode}; "
            f"stdout={proc.stdout!r} stderr={proc.stderr!r}"
        )
    data = load_json(report)
    data["max_rss_kb"] = int(rss.read_text(encoding="utf-8").strip())
    return data


def assert_common(name, report, a):
    for key, expected in (
        ("graph_sha256", a.expected_graph_sha),
        ("trace_sha256", a.expected_trace_sha),
        ("persistent_state_budget_bytes", a.budget_bytes),
    ):
        if report.get(key) != expected:
            raise RuntimeError(f"{name} {key} mismatch: {report.get(key)!r} != {expected!r}")
    for key in ("exact_answers", "oracle_trace_identity", "online_arrival_order"):
        if report.get(key) is not True:
            raise RuntimeError(f"{name} hard gate failed: {key}")
    if report.get("future_knowledge_used") is not False:
        raise RuntimeError(f"{name} future-knowledge gate failed")
    if report.get("algorithm_ns") != report.get("query_ns", 0) + report.get("update_ns", 0):
        raise RuntimeError(f"{name} timing reconciliation failed")
    if report.get("algorithm_ns", 0) <= 0:
        raise RuntimeError(f"{name} non-positive algorithm time")
    if report.get("peak_accounted_algorithm_state_bytes", 0) > a.budget_bytes:
        raise RuntimeError(f"{name} persistent-state budget exceeded")


def assert_impl(name, report, a):
    assert_common(name, report, a)
    if name == "ades":
        if report.get("candidate_commit") != a.candidate_commit:
            raise RuntimeError("ADES candidate commit identity mismatch")
        if report.get("profile") != "ADES-V3":
            raise RuntimeError("unexpected ADES profile")
        if report.get("accounting_version") != 2:
            raise RuntimeError("ADES must use accounting v2")
    elif name == "networkit":
        if report.get("networkit_version") != "11.2.2":
            raise RuntimeError("NetworKit version mismatch")
        if report.get("networkit_commit") != NETWORKIT_COMMIT:
            raise RuntimeError("NetworKit commit mismatch")
        if report.get("upstream_algorithm_modified") is not False:
            raise RuntimeError("NetworKit upstream algorithm must remain unmodified")
        if report.get("symmetric_arc_contract") is not True:
            raise RuntimeError("NetworKit symmetric graph contract missing")
        if report.get("accounting_version") != 2:
            raise RuntimeError("NetworKit accounting v2 missing")
        if report.get("per_retained_source_accounted_bytes", 0) <= 0:
            raise RuntimeError("NetworKit source-state accounting missing")
        if report.get("logical_update_count", 0) * 2 != report.get("update_count"):
            raise RuntimeError("NetworKit logical/raw update parity failure")


def paired_ci(values):
    if not values or any(v <= 0 for v in values):
        raise RuntimeError("speedup ratios must be positive")
    logs = [math.log(v) for v in values]
    mean = statistics.fmean(logs)
    gm = math.exp(mean)
    if len(logs) < 2:
        return gm, None, None
    sd = statistics.stdev(logs)
    df = len(logs) - 1
    t = T95.get(df, 1.959963985)
    half = t * sd / math.sqrt(len(logs))
    return gm, math.exp(mean - half), math.exp(mean + half)


def classify(lo, hi):
    if lo is None or hi is None:
        return "INCONCLUSIVE"
    if lo > 1.0:
        return "ADES_FASTER"
    if hi < 1.0:
        return "NETWORKIT_FASTER"
    return "INCONCLUSIVE"


def main():
    a = parse_args()
    if a.repetitions < 2:
        raise SystemExit("repetitions must be >= 2")
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ADES_PERSISTENT_STATE_BUDGET_BYTES"] = str(a.budget_bytes)
    env["ADES_CANDIDATE_COMMIT"] = a.candidate_commit
    common_args = [a.graph, a.trace, a.oracle]

    pairs = []
    for rep in range(a.repetitions):
        order = ["networkit", "ades"] if rep % 2 == 0 else ["ades", "networkit"]
        reports = {}
        for name in order:
            exe = a.ades if name == "ades" else a.networkit
            reports[name] = run_one(name, exe, common_args, rep, out, env)
            assert_impl(name, reports[name], a)

        common_keys = (
            "graph_sha256", "trace_sha256", "operation_count", "query_count",
            "update_count", "increase_count", "decrease_count",
            "unique_query_sources", "persistent_state_budget_bytes",
        )
        for key in common_keys:
            if reports["ades"].get(key) != reports["networkit"].get(key):
                raise RuntimeError(f"matched-pair parity failure for {key}")

        n = reports["networkit"]
        d = reports["ades"]
        pairs.append({
            "repetition": rep,
            "execution_order": order,
            "networkit_algorithm_ns": n["algorithm_ns"],
            "ades_algorithm_ns": d["algorithm_ns"],
            "networkit_query_ns": n["query_ns"],
            "ades_query_ns": d["query_ns"],
            "networkit_update_ns": n["update_ns"],
            "ades_update_ns": d["update_ns"],
            "networkit_over_ades": n["algorithm_ns"] / d["algorithm_ns"],
            "networkit_over_ades_query": n["query_ns"] / d["query_ns"],
            "networkit_over_ades_update": n["update_ns"] / d["update_ns"] if d["update_ns"] else None,
            "networkit_max_rss_kb": n["max_rss_kb"],
            "ades_max_rss_kb": d["max_rss_kb"],
            "networkit_peak_accounted_state_bytes": n["peak_accounted_algorithm_state_bytes"],
            "ades_peak_accounted_state_bytes": d["peak_accounted_algorithm_state_bytes"],
            "networkit_cache_hits": n["cache_hits"],
            "networkit_cache_misses": n["cache_misses"],
            "networkit_cache_evictions": n["cache_evictions"],
        })

    ratios = [p["networkit_over_ades"] for p in pairs]
    qratios = [p["networkit_over_ades_query"] for p in pairs]
    uratios = [p["networkit_over_ades_update"] for p in pairs if p["networkit_over_ades_update"] is not None]
    gm, lo, hi = paired_ci(ratios)
    qgm, qlo, qhi = paired_ci(qratios)
    ugm, ulo, uhi = paired_ci(uratios) if uratios else (None, None, None)
    first_n = load_json(out / "rep-00-networkit.json")
    first_d = load_json(out / "rep-00-ades.json")

    summary = {
        "schema": "ades-pr68-networkit-matched-pair-v1",
        "candidate_commit": a.candidate_commit,
        "networkit_commit": NETWORKIT_COMMIT,
        "graph_sha256": a.expected_graph_sha,
        "trace_sha256": a.expected_trace_sha,
        "operation_count": first_d["operation_count"],
        "query_count": first_d["query_count"],
        "update_count": first_d["update_count"],
        "logical_update_count": first_n["logical_update_count"],
        "increase_count": first_d["increase_count"],
        "decrease_count": first_d["decrease_count"],
        "unique_query_sources": first_d["unique_query_sources"],
        "exactness": "PASS",
        "repetitions": a.repetitions,
        "persistent_state_budget_bytes": a.budget_bytes,
        "paired_speedups_networkit_over_ades": ratios,
        "geomean_networkit_over_ades": gm,
        "ci95_networkit_over_ades": [lo, hi],
        "verdict": classify(lo, hi),
        "paired_wins_for_ades": sum(1 for x in ratios if x > 1.0),
        "paired_losses_for_ades": sum(1 for x in ratios if x < 1.0),
        "query_geomean_networkit_over_ades": qgm,
        "query_ci95_networkit_over_ades": [qlo, qhi],
        "query_verdict": classify(qlo, qhi),
        "update_geomean_networkit_over_ades": ugm,
        "update_ci95_networkit_over_ades": [ulo, uhi],
        "networkit_peak_accounted_state_bytes_max": max(p["networkit_peak_accounted_state_bytes"] for p in pairs),
        "ades_peak_accounted_state_bytes_max": max(p["ades_peak_accounted_state_bytes"] for p in pairs),
        "networkit_peak_rss_kb_max": max(p["networkit_max_rss_kb"] for p in pairs),
        "ades_peak_rss_kb_max": max(p["ades_max_rss_kb"] for p in pairs),
        "networkit_per_retained_source_accounted_bytes": first_n["per_retained_source_accounted_bytes"],
        "networkit_cache_hits": first_n["cache_hits"],
        "networkit_cache_misses": first_n["cache_misses"],
        "networkit_cache_evictions": first_n["cache_evictions"],
        "statistical_scope": "95% t interval over order-balanced paired log-ratios characterizes execution variability on this one frozen cell only; workload-generalization inference is performed at the cell level by the PR68 holdout runner",
    }

    with open(out / "pairs.json", "w", encoding="utf-8") as f:
        json.dump(pairs, f, indent=2, sort_keys=True); f.write("\n")
    with open(out / "summary.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, sort_keys=True); f.write("\n")
    with open(out / "pairs.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(pairs[0].keys()))
        w.writeheader(); w.writerows(pairs)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
