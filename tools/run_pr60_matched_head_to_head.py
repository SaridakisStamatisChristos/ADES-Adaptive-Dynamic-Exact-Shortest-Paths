#!/usr/bin/env python3
import argparse
import csv
import json
import math
import os
import pathlib
import statistics
import subprocess
import sys

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
    p.add_argument("--boost", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--repetitions", type=int, default=6)
    p.add_argument("--budget-bytes", type=int, default=67108864)
    p.add_argument("--candidate-commit", required=True)
    p.add_argument("--expected-graph-sha", required=True)
    p.add_argument("--expected-trace-sha", required=True)
    return p.parse_args()


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def run_one(name, exe, args, rep, out_dir, env):
    report = out_dir / f"rep-{rep:02d}-{name}.json"
    rss = out_dir / f"rep-{rep:02d}-{name}-max-rss-kb.txt"
    stdout = out_dir / f"rep-{rep:02d}-{name}.stdout.txt"
    stderr = out_dir / f"rep-{rep:02d}-{name}.stderr.txt"
    cmd = ["/usr/bin/time", "-f", "%M", "-o", str(rss), exe, *args, str(report)]
    proc = subprocess.run(cmd, env=env, text=True, capture_output=True)
    stdout.write_text(proc.stdout, encoding="utf-8")
    stderr.write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"{name} repetition {rep} failed with exit {proc.returncode}; "
            f"see {stderr}"
        )
    data = load_json(report)
    data["max_rss_kb"] = int(rss.read_text(encoding="utf-8").strip())
    return data


def assert_report(name, report, a):
    expected = {
        "graph_sha256": a.expected_graph_sha,
        "trace_sha256": a.expected_trace_sha,
        "persistent_state_budget_bytes": a.budget_bytes,
    }
    for key, value in expected.items():
        if report.get(key) != value:
            raise RuntimeError(
                f"{name} {key} mismatch: {report.get(key)!r} != {value!r}"
            )
    for key in ("exact_answers", "oracle_trace_identity", "online_arrival_order"):
        if report.get(key) is not True:
            raise RuntimeError(f"{name} hard gate failed: {key}")
    if report.get("future_knowledge_used") is not False:
        raise RuntimeError(f"{name} future-knowledge gate failed")
    if report.get("algorithm_ns") != report.get("query_ns", 0) + report.get("update_ns", 0):
        raise RuntimeError(f"{name} timing reconciliation failed")
    if report.get("peak_accounted_algorithm_state_bytes", 0) > a.budget_bytes:
        raise RuntimeError(f"{name} persistent-state budget exceeded")
    if report.get("algorithm_ns", 0) <= 0:
        raise RuntimeError(f"{name} non-positive algorithm time")
    if name == "ades":
        if report.get("candidate_commit") != a.candidate_commit:
            raise RuntimeError("ADES candidate commit identity mismatch")
        if report.get("profile") != "ADES-V3":
            raise RuntimeError("unexpected ADES profile")
        if report.get("accounting_version") != 2:
            raise RuntimeError("ADES must use PR54 accounting v2")
    elif name == "boost":
        if report.get("accounted_algorithm_state_bytes") != 0 or report.get("peak_accounted_algorithm_state_bytes") != 0:
            raise RuntimeError("Boost recomputation unexpectedly retained adaptive state")


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
        return "IMPROVEMENT"
    if hi < 1.0:
        return "REGRESSION"
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
        order = ["boost", "ades"] if rep % 2 == 0 else ["ades", "boost"]
        reports = {}
        for name in order:
            exe = a.boost if name == "boost" else a.ades
            reports[name] = run_one(name, exe, common_args, rep, out, env)
            assert_report(name, reports[name], a)

        common_keys = (
            "graph_sha256", "trace_sha256", "operation_count", "query_count",
            "update_count", "increase_count", "decrease_count",
            "unique_query_sources", "persistent_state_budget_bytes",
        )
        for key in common_keys:
            if reports["ades"].get(key) != reports["boost"].get(key):
                raise RuntimeError(f"matched-pair parity failure for {key}")

        b = reports["boost"]
        d = reports["ades"]
        pair = {
            "repetition": rep,
            "execution_order": order,
            "boost_algorithm_ns": b["algorithm_ns"],
            "ades_algorithm_ns": d["algorithm_ns"],
            "boost_query_ns": b["query_ns"],
            "ades_query_ns": d["query_ns"],
            "boost_update_ns": b["update_ns"],
            "ades_update_ns": d["update_ns"],
            "boost_max_rss_kb": b["max_rss_kb"],
            "ades_max_rss_kb": d["max_rss_kb"],
            "boost_over_ades": b["algorithm_ns"] / d["algorithm_ns"],
            "boost_over_ades_query": b["query_ns"] / d["query_ns"],
            "boost_over_ades_update": b["update_ns"] / d["update_ns"] if d["update_ns"] else None,
            "ades_accounted_state_bytes": d["accounted_algorithm_state_bytes"],
            "ades_peak_accounted_state_bytes": d["peak_accounted_algorithm_state_bytes"],
            "boost_accounted_state_bytes": b["accounted_algorithm_state_bytes"],
            "boost_peak_accounted_state_bytes": b["peak_accounted_algorithm_state_bytes"],
        }
        pairs.append(pair)

    ratios = [p["boost_over_ades"] for p in pairs]
    query_ratios = [p["boost_over_ades_query"] for p in pairs]
    update_ratios = [p["boost_over_ades_update"] for p in pairs if p["boost_over_ades_update"] is not None]
    gm, lo, hi = paired_ci(ratios)
    qgm, qlo, qhi = paired_ci(query_ratios)
    ugm, ulo, uhi = paired_ci(update_ratios) if update_ratios else (None, None, None)

    first_ades = load_json(out / "rep-00-ades.json")
    first_boost = load_json(out / "rep-00-boost.json")
    summary = {
        "schema": "ades-pr60-matched-head-to-head-v1",
        "candidate_commit": a.candidate_commit,
        "graph_sha256": a.expected_graph_sha,
        "trace_sha256": a.expected_trace_sha,
        "operation_count": first_ades["operation_count"],
        "query_count": first_ades["query_count"],
        "update_count": first_ades["update_count"],
        "increase_count": first_ades["increase_count"],
        "decrease_count": first_ades["decrease_count"],
        "unique_query_sources": first_ades["unique_query_sources"],
        "exactness": "PASS",
        "repetitions": a.repetitions,
        "persistent_state_budget_bytes": a.budget_bytes,
        "ades_accounting_version": first_ades["accounting_version"],
        "boost_native_accounting_version": first_boost["accounting_version"],
        "common_accounting_interpretation": "PR54 v2; Boost retains zero adaptive state, therefore its accounted value is 0 under both PR43 v1 and PR54 v2",
        "paired_speedups_boost_over_ades": ratios,
        "geomean_boost_over_ades": gm,
        "ci95_boost_over_ades": [lo, hi],
        "verdict": classify(lo, hi),
        "paired_losses_for_ades": sum(1 for x in ratios if x < 1.0),
        "paired_wins_for_ades": sum(1 for x in ratios if x > 1.0),
        "query_geomean_boost_over_ades": qgm,
        "query_ci95_boost_over_ades": [qlo, qhi],
        "update_geomean_boost_over_ades": ugm,
        "update_ci95_boost_over_ades": [ulo, uhi],
        "ades_peak_accounted_state_bytes_max": max(p["ades_peak_accounted_state_bytes"] for p in pairs),
        "boost_peak_accounted_state_bytes_max": 0,
        "ades_peak_rss_kb_max": max(p["ades_max_rss_kb"] for p in pairs),
        "boost_peak_rss_kb_max": max(p["boost_max_rss_kb"] for p in pairs),
        "statistical_scope": "95% t interval over paired log-ratios characterizes execution variability on this one frozen trace only; it is not workload-generalization evidence",
    }

    with open(out / "pairs.json", "w", encoding="utf-8") as f:
        json.dump(pairs, f, indent=2, sort_keys=True)
        f.write("\n")
    with open(out / "summary.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, sort_keys=True)
        f.write("\n")
    with open(out / "pairs.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(pairs[0].keys()))
        w.writeheader()
        w.writerows(pairs)

    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"PR60 matched benchmark failed: {exc}", file=sys.stderr)
        raise
