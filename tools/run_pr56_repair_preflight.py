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
    "regime", "policy", "seed", "trace_sha256", "queries", "updates",
    "increases", "decreases", "source", "algorithm_ns", "rebuilds",
    "repairs", "aborts", "filtered",
)


def run(cmd: list[str]) -> str:
    p = subprocess.run(cmd, text=True, capture_output=True, timeout=1800)
    if p.returncode:
        raise RuntimeError(f"failed: {' '.join(cmd)}\n{p.stderr[-4000:]}")
    return p.stdout.strip()


def parse(line: str) -> dict[str, str]:
    parts = line.split(",")
    if len(parts) != len(FIELDS):
        raise RuntimeError(f"controller row field mismatch: {len(parts)} != {len(FIELDS)}: {line}")
    return dict(zip(FIELDS, parts))


def ci_from_seed_logs(seed_logs: list[float]) -> tuple[float, float, float]:
    mean = statistics.mean(seed_logs)
    if len(seed_logs) < 2:
        return math.exp(mean), math.exp(mean), math.exp(mean)
    sd = statistics.stdev(seed_logs)
    half = T95_DF2 * sd / math.sqrt(len(seed_logs))
    return math.exp(mean), math.exp(mean - half), math.exp(mean + half)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--candidate", required=True)
    ap.add_argument("--graph", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--rounds", type=int, default=40)
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    seeds = (7, 17, 29)
    regimes = ("small", "catastrophic")
    rows: list[dict[str, object]] = []

    for regime in regimes:
        for seed in seeds:
            for rep in range(2):
                order = ("baseline", "candidate") if (seed + rep) % 2 else ("candidate", "baseline")
                results: dict[str, dict[str, str]] = {}
                for variant in order:
                    exe = args.baseline if variant == "baseline" else args.candidate
                    row = parse(run([exe, args.graph, regime, str(args.rounds), str(seed), "work"]))
                    results[variant] = row
                    rows.append({"variant": variant, "repetition": rep, **row})

                b = results["baseline"]
                c = results["candidate"]
                identity = ("trace_sha256", "queries", "updates", "increases", "decreases", "source")
                for key in identity:
                    if b[key] != c[key]:
                        raise RuntimeError(f"matched identity failure {regime} seed={seed} rep={rep} field={key}: {b[key]} != {c[key]}")

    with (out / "measurements.csv").open("w", newline="") as f:
        fieldnames = ["variant", "repetition", *FIELDS]
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(rows)

    summary = []
    for regime in regimes:
        seed_logs = []
        mechanism = {}
        for seed in seeds:
            rep_logs = []
            for rep in range(2):
                b = next(r for r in rows if r["regime"] == regime and r["seed"] == str(seed) and r["repetition"] == rep and r["variant"] == "baseline")
                c = next(r for r in rows if r["regime"] == regime and r["seed"] == str(seed) and r["repetition"] == rep and r["variant"] == "candidate")
                rep_logs.append(math.log(int(b["algorithm_ns"]) / int(c["algorithm_ns"])))
                mechanism.setdefault(seed, []).append({
                    "baseline_rebuilds": int(b["rebuilds"]),
                    "candidate_rebuilds": int(c["rebuilds"]),
                    "baseline_repairs": int(b["repairs"]),
                    "candidate_repairs": int(c["repairs"]),
                    "baseline_aborts": int(b["aborts"]),
                    "candidate_aborts": int(c["aborts"]),
                })
            seed_logs.append(statistics.mean(rep_logs))

        gm, lo, hi = ci_from_seed_logs(seed_logs)
        if lo > 1.01:
            result = "improvement"
        elif hi < 0.99:
            result = "regression"
        else:
            result = "inconclusive"
        summary.append({
            "regime": regime,
            "geomean_baseline_over_candidate": gm,
            "ci95_low": lo,
            "ci95_high": hi,
            "result": result,
            "seed_speedups": [math.exp(x) for x in seed_logs],
            "mechanism": mechanism,
        })

    decision = {
        "baseline": "PR55 radix queue",
        "candidate": "PR56 predictive repair preflight",
        "rounds_per_execution": args.rounds,
        "independent_seeds": list(seeds),
        "repetitions_per_seed": 2,
        "results": summary,
        "improvements": sum(x["result"] == "improvement" for x in summary),
        "regressions": sum(x["result"] == "regression" for x in summary),
        "inconclusive": sum(x["result"] == "inconclusive" for x in summary),
    }
    (out / "decision.json").write_text(json.dumps(decision, indent=2) + "\n")

    md = [
        "# PR56 predictive repair preflight — matched development result",
        "",
        "`baseline / candidate > 1` means PR56 is faster.",
        "",
        "| regime | speedup | 95% CI | result |",
        "|---|---:|---:|---|",
    ]
    for x in summary:
        md.append(f"| {x['regime']} | {x['geomean_baseline_over_candidate']:.6f}x | {x['ci95_low']:.6f}–{x['ci95_high']:.6f} | {x['result']} |")
    md += ["", "Performance losses and inconclusive cells are preserved as first-class development evidence.", ""]
    (out / "SUMMARY.md").write_text("\n".join(md))
    print(json.dumps(decision, indent=2))


if __name__ == "__main__":
    main()
