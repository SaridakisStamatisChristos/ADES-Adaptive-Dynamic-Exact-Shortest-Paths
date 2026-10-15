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
FIELDS = ("shape", "vertices", "rounds", "seed", "total_ns", "mean_ns", "mean_discovered", "checksum")


def run(cmd, timeout=1200):
    p = subprocess.run([str(x) for x in cmd], text=True, capture_output=True, timeout=timeout)
    if p.returncode:
        raise RuntimeError(f"failed ({p.returncode}): {' '.join(map(str, cmd))}\n{p.stderr[-8000:]}")
    return p.stdout.strip()


def parse_row(text):
    parts = next(csv.reader([text]))
    if len(parts) != len(FIELDS):
        raise RuntimeError(f"field mismatch: {len(parts)} != {len(FIELDS)}")
    return dict(zip(FIELDS, parts))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--candidate", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    baseline = Path(args.baseline).resolve()
    candidate = Path(args.candidate).resolve()
    out = Path(args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    cases = (("chain", 4096, 200), ("binary", 65536, 60))
    seeds = (7, 17, 29)
    repeats = 2
    rows = []

    for shape, vertices, rounds in cases:
        for seed in seeds:
            for rep in range(repeats):
                versions = (("pr55", baseline), ("pr57", candidate))
                if rep & 1:
                    versions = tuple(reversed(versions))
                for version, runner in versions:
                    row = parse_row(run([runner, shape, vertices, rounds, seed]))
                    if row["shape"] != shape or int(row["vertices"]) != vertices or int(row["seed"]) != seed:
                        raise RuntimeError("benchmark identity mismatch")
                    rows.append(dict(case=f"{shape}-{vertices}", repeat=rep, version=version, **row))

    with (out / "measurements.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

    paired = {}
    for row in rows:
        key = (row["case"], int(row["seed"]), int(row["repeat"]))
        paired.setdefault(key, {})[row["version"]] = int(row["total_ns"])

    by_seed = {}
    for (case, seed, rep), pair in paired.items():
        if set(pair) != {"pr55", "pr57"}:
            raise RuntimeError("incomplete version pair")
        if pair["pr57"] <= 0:
            raise RuntimeError("candidate timing is zero")
        by_seed.setdefault((case, seed), []).append(math.log(pair["pr55"] / pair["pr57"]))

    summary = []
    for shape, vertices, _ in cases:
        case = f"{shape}-{vertices}"
        seed_logs = [statistics.mean(by_seed[(case, seed)]) for seed in seeds]
        mean = statistics.mean(seed_logs); sd = statistics.stdev(seed_logs)
        half = T95_DF2 * sd / math.sqrt(len(seed_logs))
        gm = math.exp(mean); lo = math.exp(mean-half); hi = math.exp(mean+half)
        verdict = "IMPROVEMENT" if lo > 1.0 else "REGRESSION" if hi < 1.0 else "INCONCLUSIVE"
        baseline_discovered = [int(r["mean_discovered"]) for r in rows if r["case"] == case and r["version"] == "pr55"]
        candidate_discovered = [int(r["mean_discovered"]) for r in rows if r["case"] == case and r["version"] == "pr57"]
        summary.append(dict(
            case=case,
            pr55_over_pr57_geomean=f"{gm:.6f}",
            ci95_low=f"{lo:.6f}",
            ci95_high=f"{hi:.6f}",
            verdict=verdict,
            pr55_mean_discovered=f"{statistics.mean(baseline_discovered):.1f}",
            pr57_mean_discovered=f"{statistics.mean(candidate_discovered):.1f}",
        ))

    with (out / "summary.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0])); w.writeheader(); w.writerows(summary)

    decision = {
        "classification": "PR57 mechanism ablation; PR55 is causal baseline",
        "independent_seeds": list(seeds),
        "repeats_per_seed": repeats,
        "cases": [f"{shape}-{vertices}" for shape, vertices, _ in cases],
        "improvements": sum(r["verdict"] == "IMPROVEMENT" for r in summary),
        "regressions": sum(r["verdict"] == "REGRESSION" for r in summary),
        "inconclusive": sum(r["verdict"] == "INCONCLUSIVE" for r in summary),
    }
    (out / "decision.json").write_text(json.dumps(decision, indent=2) + "\n")
    print(json.dumps({"decision": decision, "summary": summary}, indent=2))


if __name__ == "__main__":
    main()
