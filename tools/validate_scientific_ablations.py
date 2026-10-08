#!/usr/bin/env python3
"""Validate one six-profile PR45 scientific-ablation cell and summarize effects."""

import argparse
import json
from pathlib import Path

from phase_schema import parse_phase_line, validate_v2_reconciliation

PROFILES = (
    "COLD",
    "FREQ-LRU-REBUILD",
    "FREQ-LRU-REPAIR",
    "WORK-LRU-REPAIR",
    "WORK-DEBT-REPAIR",
    "B4",
)


def ratio(a, b):
    return None if b == 0 else a / b


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()

    lines = [line for line in Path(args.input).read_text().splitlines() if line.strip()]
    if len(lines) != len(PROFILES):
        raise SystemExit(f"expected {len(PROFILES)} rows, got {len(lines)}")

    rows = []
    for line in lines:
        row = parse_phase_line(line)
        validate_v2_reconciliation(row)
        if row["schema_version"] != "2":
            raise SystemExit("PR45 requires schema_version=2")
        rows.append(row)

    by_name = {row["baseline"]: row for row in rows}
    if set(by_name) != set(PROFILES) or len(by_name) != len(PROFILES):
        raise SystemExit(f"profile set mismatch: {sorted(by_name)}")

    identity_fields = (
        "family", "seed", "trace_sha256", "queries", "updates",
        "increase_count", "decrease_count", "update_every", "hot_sources", "epoch",
        "persistent_state_budget_bytes", "accounting_version",
    )
    identities = {tuple(row[field] for field in identity_fields) for row in rows}
    if len(identities) != 1:
        raise SystemExit("ablation rows are not one matched benchmark cell")

    cold = by_name["COLD"]
    for field in ("resident_queries", "promotions", "evictions",
                  "accounted_algorithm_state_bytes", "peak_accounted_algorithm_state_bytes"):
        if int(cold[field]) != 0:
            raise SystemExit(f"COLD invariant failed: {field}={cold[field]}")

    simple_profiles = (
        "COLD", "FREQ-LRU-REBUILD", "FREQ-LRU-REPAIR",
        "WORK-LRU-REPAIR", "WORK-DEBT-REPAIR",
    )
    for profile in simple_profiles:
        row = by_name[profile]
        if int(row["cooldown_blocks"]) != 0:
            raise SystemExit(f"{profile} unexpectedly exercised cooldown")
        if int(row["admission_rejections"]) != 0:
            raise SystemExit(f"{profile} unexpectedly exercised hysteresis rejection")

    rebuild = by_name["FREQ-LRU-REBUILD"]
    for field in ("filtered_updates", "decrease_repairs", "increase_repairs", "repair_aborts"):
        if int(rebuild[field]) != 0:
            raise SystemExit(f"FREQ-LRU-REBUILD unexpectedly used repair: {field}={rebuild[field]}")

    times = {name: int(by_name[name]["algorithm_ns"]) for name in PROFILES}
    stage_pairs = list(zip(PROFILES, PROFILES[1:]))
    stage_ratios = {
        f"{left}_over_{right}": ratio(times[left], times[right])
        for left, right in stage_pairs
    }
    full_vs_simple = ratio(times["FREQ-LRU-REPAIR"], times["B4"])
    fastest = min(PROFILES, key=lambda name: times[name])

    summary = {
        "schema": "ades-pr45-scientific-ablation-v1",
        "claim_ids": ["C5", "C6", "C7", "C9"],
        "status": "PASS",
        "interpretation_rule": "No favorable-result requirement; negative and neutral ablations remain admissible evidence.",
        "trace_sha256": cold["trace_sha256"],
        "family": cold["family"],
        "seed": int(cold["seed"]),
        "queries": int(cold["queries"]),
        "updates": int(cold["updates"]),
        "increase_count": int(cold["increase_count"]),
        "decrease_count": int(cold["decrease_count"]),
        "persistent_state_budget_bytes": int(cold["persistent_state_budget_bytes"]),
        "algorithm_ns": times,
        "stage_speedup_ratios": stage_ratios,
        "freq_lru_repair_over_full_ades": full_vs_simple,
        "full_ades_faster_than_freq_lru_repair": (
            None if full_vs_simple is None else full_vs_simple > 1.0
        ),
        "fastest_profile_in_this_cell": fastest,
        "mechanism_counters": {
            name: {
                key: int(by_name[name][key])
                for key in (
                    "cold_queries", "resident_queries", "promotions", "evictions",
                    "rebuilds", "repair_aborts", "cooldown_blocks",
                    "admission_rejections", "filtered_updates",
                    "decrease_repairs", "increase_repairs",
                )
            }
            for name in PROFILES
        },
    }

    text = json.dumps(summary, indent=2) + "\n"
    if args.output:
        Path(args.output).write_text(text)
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
