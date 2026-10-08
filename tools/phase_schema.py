#!/usr/bin/env python3
"""Versioned parser for ades_phase CSV rows."""

PHASE_FIELDS_V1 = (
    "baseline", "family", "seed", "trace_sha256", "queries", "updates",
    "increase_count", "decrease_count", "update_every", "hot_sources", "epoch",
    "cap", "persistent_state_budget_bytes", "accounted_algorithm_state_bytes",
    "peak_accounted_algorithm_state_bytes", "accounting_version", "algorithm_ns",
    "cold_queries", "resident_queries", "promotions", "evictions", "rebuilds",
    "repair_aborts",
)

PHASE_FIELDS_V2 = (
    "schema_version", "baseline", "family", "seed", "trace_sha256", "queries",
    "updates", "increase_count", "decrease_count", "update_every", "hot_sources",
    "epoch", "cap", "persistent_state_budget_bytes",
    "accounted_algorithm_state_bytes", "peak_accounted_algorithm_state_bytes",
    "accounting_version", "oracle_ns", "algorithm_ns", "query_ns", "update_ns",
    "operation_p50_ns", "operation_p95_ns", "query_p50_ns", "query_p95_ns",
    "update_p50_ns", "update_p95_ns", "cold_queries", "resident_queries",
    "promotions", "evictions", "rebuilds", "repair_aborts", "cooldown_blocks",
    "admission_rejections", "filtered_updates", "decrease_repairs",
    "increase_repairs", "memory_budget_rejections", "memory_metadata_prunes",
    "b4_query_time_ns", "b4_update_time_ns", "b4_cold_query_ns",
    "b4_resident_query_ns", "b4_query_policy_ns", "b4_promotion_ns",
    "b4_eviction_ns", "b4_graph_update_ns", "b4_decrease_repair_ns",
    "b4_increase_repair_ns", "b4_rebuild_ns", "b4_controller_ns",
    "b4_update_policy_ns",
)


def parse_phase_line(line):
    parts = line.rstrip("\n").split(",")
    if len(parts) == len(PHASE_FIELDS_V2) and parts[0] == "2":
        return dict(zip(PHASE_FIELDS_V2, parts))
    if len(parts) == len(PHASE_FIELDS_V1):
        row = dict(zip(PHASE_FIELDS_V1, parts))
        row["schema_version"] = "1"
        return row
    raise ValueError(f"unsupported ades_phase row with {len(parts)} fields")


def validate_v2_reconciliation(row):
    if row.get("schema_version") != "2":
        return
    algorithm = int(row["algorithm_ns"])
    query = int(row["query_ns"])
    update = int(row["update_ns"])
    if algorithm != query + update:
        raise ValueError("algorithm_ns does not equal query_ns + update_ns")
    if row["baseline"] != "B4":
        return
    b4_query = int(row["b4_query_time_ns"])
    b4_update = int(row["b4_update_time_ns"])
    query_parts = sum(int(row[name]) for name in (
        "b4_cold_query_ns", "b4_resident_query_ns", "b4_query_policy_ns",
        "b4_promotion_ns", "b4_eviction_ns",
    ))
    update_parts = sum(int(row[name]) for name in (
        "b4_graph_update_ns", "b4_decrease_repair_ns", "b4_increase_repair_ns",
        "b4_rebuild_ns", "b4_controller_ns", "b4_update_policy_ns",
    ))
    if b4_query != query_parts:
        raise ValueError("B4 query components do not reconcile")
    if b4_update != update_parts:
        raise ValueError("B4 update components do not reconcile")
