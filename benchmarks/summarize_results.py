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
        # PR42 publication runs use trace_sha256. Retain trace_hash support only
        # so previously archived engineering evidence remains readable.
        if "trace_sha256" in reader.fieldnames:
            trace_field = "trace_sha256"
        elif "trace_hash" in reader.fieldnames:
            trace_field = "trace_hash"
        else:
            raise SystemExit(f"missing trace identity column in {path}")

        required = {"baseline", "rep", "seed", "workload", trace_field, "ns", "max_rss_kb"}
        missing = required.difference(reader.fieldnames)
        if missing:
            raise SystemExit(f"missing matrix columns in {path}: {sorted(missing)}")

        by_trace = defaultdict(set)
        for row in rows:
            by_trace[(row["seed"], row["workload"], row["rep"])].add(row[trace_field])
        bad = [key for key, values in by_trace.items() if len(values) != 1]
        if bad:
            raise SystemExit(f"trace identity mismatch in {path}: {bad[:3]}")

        count_fields = ["query_count", "update_count", "increase_count", "decrease_count"]
        present_counts = [field for field in count_fields if field in reader.fieldnames]
        if present_counts and len(present_counts) != len(count_fields):
            missing_counts = sorted(set(count_fields).difference(reader.fieldnames))
            raise SystemExit(f"incomplete PR42 trace counts in {path}: {missing_counts}")
        if present_counts:
            by_counts = defaultdict(set)
            for row in rows:
                key = (row["seed"], row["workload"], row["rep"])
                by_counts[key].add(tuple(row[field] for field in count_fields))
            bad_counts = [key for key, values in by_counts.items() if len(values) != 1]
            if bad_counts:
                raise SystemExit(f"trace operation-count mismatch in {path}: {bad_counts[:3]}")

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
