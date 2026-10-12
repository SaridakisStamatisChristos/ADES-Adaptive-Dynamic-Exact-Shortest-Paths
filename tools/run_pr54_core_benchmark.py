#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import subprocess
from pathlib import Path

T95_DF2 = 4.302652729911275
FIELDS = (
    "profile","trace_sha256","queries","updates","increases","decreases","budget_bytes",
    "algorithm_ns","query_ns","update_ns","op_p50_ns","op_p95_ns","query_p50_ns",
    "query_p95_ns","update_p50_ns","update_p95_ns","current_bytes","peak_bytes",
    "cold_queries","resident_queries","promotions","evictions","rebuilds","repair_aborts",
    "cooldown_blocks","admission_rejections","filtered_updates","decrease_repairs",
    "increase_repairs","memory_budget_rejections","memory_metadata_prunes",
)


def run(cmd, timeout=900):
    p = subprocess.run([str(x) for x in cmd], text=True, capture_output=True, timeout=timeout)
    if p.returncode:
        raise RuntimeError(f"failed ({p.returncode}): {' '.join(map(str, cmd))}\n{p.stderr[-6000:]}")
    return p.stdout.strip()


def parse_row(text):
    parts = next(csv.reader([text]))
    if len(parts) != len(FIELDS):
        raise RuntimeError(f"runner field mismatch: {len(parts)} != {len(FIELDS)}: {text}")
    return dict(zip(FIELDS, parts))


def cases():
    mib = 1024 * 1024
    return [
        dict(id="grid-hotpool-u10", family="hot-pool", interval=10,
             mode="balanced-random", magnitude="small", locality=70, budget=24*mib),
        dict(id="grid-churn-u5", family="churn", interval=5,
             mode="strict-alternating", magnitude="medium", locality=100, budget=16*mib),
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline-runner", required=True)
    ap.add_argument("--candidate-runner", required=True)
    ap.add_argument("--generator", required=True)
    ap.add_argument("--graph", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    baseline = Path(args.baseline_runner).resolve()
    candidate = Path(args.candidate_runner).resolve()
    generator = Path(args.generator).resolve()
    graph = Path(args.graph).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    traces = out / "traces"; traces.mkdir(exist_ok=True)
    oracles = out / "oracles"; oracles.mkdir(exist_ok=True)

    profiles = ("COLD", "FREQ-LRU-REPAIR", "ADES-V3")
    seeds = (7, 17, 29)
    repeats = 2
    rows = []

    for ci, case in enumerate(cases()):
        for seed in seeds:
            stem = f"{ci:02d}-{case['id']}-s{seed}"
            trace = traces / f"{stem}.trace"
            meta = traces / f"{stem}.json"
            oracle = oracles / f"{stem}.oracle"
            run([generator, graph, trace, meta, seed, 160, case["family"], case["interval"],
                 case["mode"], case["magnitude"], 4, 15, case["locality"], 4])
            run([baseline, "oracle", graph, trace, oracle])
            trace_sha = json.loads(meta.read_text())["trace_sha256"]

            for profile in profiles:
                for rep in range(repeats):
                    versions = (("baseline", baseline), ("candidate", candidate))
                    if rep & 1:
                        versions = tuple(reversed(versions))
                    for version, runner in versions:
                        row = parse_row(run([runner, "run", graph, trace, oracle,
                                             case["budget"], profile]))
                        if row["trace_sha256"] != trace_sha:
                            raise RuntimeError("trace identity mismatch")
                        if int(row["peak_bytes"]) > case["budget"]:
                            raise RuntimeError("persistent-state budget violation")
                        rows.append(dict(case=case["id"], seed=seed, repeat=rep,
                                         version=version, **row))

    measurement_path = out / "measurements.csv"
    with measurement_path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader(); w.writerows(rows)

    paired = {}
    for row in rows:
        key = (row["case"], row["profile"], int(row["seed"]), int(row["repeat"]))
        paired.setdefault(key, {})[row["version"]] = int(row["algorithm_ns"])

    by_seed = {}
    for (case, profile, seed, rep), pair in paired.items():
        if set(pair) != {"baseline", "candidate"}:
            raise RuntimeError("incomplete version pair")
        speedup = pair["baseline"] / pair["candidate"]
        by_seed.setdefault((case, profile, seed), []).append(math.log(speedup))

    summary = []
    for case in (c["id"] for c in cases()):
        for profile in profiles:
            seed_logs = []
            for seed in seeds:
                values = by_seed[(case, profile, seed)]
                seed_logs.append(statistics.mean(values))
            mean = statistics.mean(seed_logs)
            sd = statistics.stdev(seed_logs)
            half = T95_DF2 * sd / math.sqrt(len(seed_logs))
            gm = math.exp(mean); lo = math.exp(mean-half); hi = math.exp(mean+half)
            verdict = "IMPROVEMENT" if lo > 1.0 else "REGRESSION" if hi < 1.0 else "INCONCLUSIVE"
            summary.append(dict(case=case, profile=profile,
                                baseline_over_candidate_geomean=f"{gm:.6f}",
                                ci95_low=f"{lo:.6f}", ci95_high=f"{hi:.6f}",
                                verdict=verdict))

    with (out / "summary.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0]))
        w.writeheader(); w.writerows(summary)

    decision = {
        "classification": "development evidence only",
        "independent_seeds": list(seeds),
        "repeats_per_seed": repeats,
        "cases": len(cases()),
        "profiles": list(profiles),
        "improvements": sum(r["verdict"] == "IMPROVEMENT" for r in summary),
        "regressions": sum(r["verdict"] == "REGRESSION" for r in summary),
        "inconclusive": sum(r["verdict"] == "INCONCLUSIVE" for r in summary),
    }
    (out / "decision.json").write_text(json.dumps(decision, indent=2) + "\n")
    print(json.dumps({"decision": decision, "summary": summary}, indent=2))


if __name__ == "__main__":
    main()
