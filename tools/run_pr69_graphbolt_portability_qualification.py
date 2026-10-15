#!/usr/bin/env python3
"""Canonical PR69 qualification for the documented GraphBolt portability patch.

This wrapper deliberately reuses the frozen PR69 graph generator, update stream,
execution contract, and fresh-Dijkstra oracle from
run_pr69_graphbolt_qualification.py. It changes only comparator provenance and
the qualification label so a portability-patched GraphBolt build can never be
confused with pristine upstream.

No performance claim is made here.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path

_BASE_PATH = Path(__file__).with_name("run_pr69_graphbolt_qualification.py")
_BASE_SPEC = importlib.util.spec_from_file_location("pr69_frozen_qualification", _BASE_PATH)
if _BASE_SPEC is None or _BASE_SPEC.loader is None:
    raise RuntimeError(f"cannot load frozen PR69 harness: {_BASE_PATH}")
base = importlib.util.module_from_spec(_BASE_SPEC)
_BASE_SPEC.loader.exec_module(base)

PATCH_ID = "kickstarter-missing-return-v1"
PATCH_SCOPE = (
    "GraphBolt core/graphBolt/KickStarterEngine.h: add explicit `return 0;` "
    "at the end of no-argument traditionalIncrementalComputation(); the SSSP "
    "caller invokes it only for side effects and discards the return value"
)
QUALIFICATION = (
    "QUALIFIED_DIRECTED_FIXED_SOURCE_WEIGHTED_STREAMING_SSSP_"
    "WITH_UINT16_DISTANCE_BOUND_PORTABILITY_PATCHED"
)
SCHEMA = "ades-pr69-graphbolt-weighted-sssp-portability-qualification-v2"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--graphbolt", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--portability-patch-id", default=PATCH_ID, choices=[PATCH_ID])
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    cases = []
    for seed in base.SEEDS:
        for source in base.SOURCES:
            case = base.run_case(args.graphbolt, out / "cases", seed, source)
            cases.append(case)
            if case.get("execution_failed"):
                break
        if cases and cases[-1].get("execution_failed"):
            break

    planned = len(base.SEEDS) * len(base.SOURCES)
    full_matrix_executed = len(cases) == planned
    all_exact = full_matrix_executed and all(c.get("exact") for c in cases)
    checks = sum(c.get("checks", 0) for c in cases)

    result = {
        "schema": SCHEMA,
        "base_harness_schema": "ades-pr69-graphbolt-weighted-sssp-qualification-v1",
        "graphbolt_commit": base.GRAPHBOLT_COMMIT,
        "upstream_algorithm_modified": True,
        "modification_class": "PORTABILITY_PATCH_ONLY",
        "portability_patch_id": args.portability_patch_id,
        "portability_patch_scope": PATCH_SCOPE,
        "semantic_matrix_changed_from_preregistered_pr69": False,
        "semantic_scope": (
            "directed fixed-source positive-integer weighted SSSP under edge "
            "add/delete; logical weight replacement encoded as delete(old)+add(new)"
        ),
        "application_distance_boundary": base.MAX_DISTANCE,
        "vertices": base.N,
        "initial_edges": base.INITIAL_EDGES,
        "seeds": base.SEEDS,
        "sources": base.SOURCES,
        "replacements_per_case": base.REPLACEMENTS,
        "case_timeout_seconds": base.CASE_TIMEOUT_SECONDS,
        "planned_case_count": planned,
        "case_count": len(cases),
        "full_matrix_executed": full_matrix_executed,
        "exact_case_count": sum(1 for c in cases if c.get("exact")),
        "checks": checks,
        "all_exact": all_exact,
        "qualification": QUALIFICATION if all_exact else "NOT_QUALIFIED",
        "first_failure": next((c for c in cases if not c.get("exact")), None),
        "cases": cases,
    }

    (out / "qualification.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps({k: result[k] for k in [
        "qualification", "portability_patch_id", "planned_case_count",
        "case_count", "full_matrix_executed", "exact_case_count", "checks",
        "first_failure"
    ]}, indent=2, sort_keys=True))
    return 0 if all_exact else 1


if __name__ == "__main__":
    raise SystemExit(main())
