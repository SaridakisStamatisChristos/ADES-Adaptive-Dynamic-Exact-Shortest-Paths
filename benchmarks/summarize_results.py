#!/usr/bin/env python3
import csv
import glob
import statistics
from collections import defaultdict
from pathlib import Path

matrix = defaultdict(list)
controller = defaultdict(list)

for path_str in glob.glob("results/*.csv"):
    path = Path(path_str)
    name = path.name
    if name.startswith("USA-road-d.NY-"):
        metric = "distance"
    elif name.startswith("USA-road-t.NY-"):
        metric = "time"
    else:
        raise SystemExit(f"unrecognized evidence filename: {name}")

    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            raise SystemExit(f"empty CSV schema: {path}")
        rows = list(reader)

    if "baseline" in reader.fieldnames:
        required = {"baseline", "rep", "seed", "workload", "trace_hash", "ns", "max_rss_kb"}
        missing = required.difference(reader.fieldnames)
        if missing:
            raise SystemExit(f"missing matrix columns in {path}: {sorted(missing)}")

        by_trace = defaultdict(set)
        for row in rows:
            by_trace[(row["seed"], row["workload"], row["rep"])].add(row["trace_hash"])
        bad = [key for key, values in by_trace.items() if len(values) != 1]
        if bad:
            raise SystemExit(f"trace fingerprint mismatch in {path}: {bad[:3]}")

        for row in rows:
            matrix[(metric, row["workload"], row["baseline"])].append(
                (int(row["ns"]), int(row["max_rss_kb"]))
            )
    elif "regime" in reader.fieldnames:
        required = {
            "regime", "policy", "ns", "max_rss_kb", "rebuilds",
            "increase_repairs", "repair_aborts"
        }
        missing = required.difference(reader.fieldnames)
        if missing:
            raise SystemExit(f"missing controller columns in {path}: {sorted(missing)}")

        for row in rows:
            controller[(metric, row["regime"], row["policy"])].append(
                (
                    int(row["ns"]),
                    int(row["max_rss_kb"]),
                    int(row["rebuilds"]),
                    int(row["increase_repairs"]),
                    int(row["repair_aborts"]),
                )
            )
    else:
        raise SystemExit(f"unknown evidence CSV schema: {path}")

def percentile(values, fraction):
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((len(ordered) - 1) * fraction))))
    return ordered[index]

with open("results/SUMMARY.md", "w") as out:
    out.write("""# ADES benchmark summary

Generated from raw evidence CSVs. Times are per isolated process measurement; median/p95 are descriptive, not confidence intervals.

## B0-B4 matrix

| metric | workload | baseline | n | median ms | p95 ms | median RSS MiB |
|---|---|---:|---:|---:|---:|---:|
""")
    for (metric, workload, baseline), values in sorted(matrix.items()):
        times = [value[0] for value in values]
        rss = [value[1] for value in values]
        out.write(
            f"| {metric} | {workload} | {baseline} | {len(values)} | "
            f"{statistics.median(times) / 1e6:.3f} | "
            f"{percentile(times, .95) / 1e6:.3f} | "
            f"{statistics.median(rss) / 1024:.2f} |" + chr(10)
        )

    out.write("""
## Controller ablation

| metric | regime | policy | n | median ms | p95 ms | median RSS MiB | rebuilds | repairs | aborts |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
""")
    for (metric, regime, policy), values in sorted(controller.items()):
        times = [value[0] for value in values]
        rss = [value[1] for value in values]
        out.write(
            f"| {metric} | {regime} | {policy} | {len(values)} | "
            f"{statistics.median(times) / 1e6:.3f} | "
            f"{percentile(times, .95) / 1e6:.3f} | "
            f"{statistics.median(rss) / 1024:.2f} | "
            f"{sum(value[2] for value in values)} | "
            f"{sum(value[3] for value in values)} | "
            f"{sum(value[4] for value in values)} |" + chr(10)
        )

print("results/SUMMARY.md")
