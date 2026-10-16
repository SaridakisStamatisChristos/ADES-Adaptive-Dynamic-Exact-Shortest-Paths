#!/usr/bin/env python3
"""Independently recompute ADES PR87 headline evidence from canonical artifact ZIPs.

This verifier deliberately ignores each artifact's archived aggregate/holdout summary.
It reads per-cell CSV rows, recomputes geometric means, log-space 95% t intervals,
loss counts, minima, exactness counts, and selected cache/state facts, then compares
those independent results with the pinned PR87 publication manifest.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

T975_DF23 = 2.0686576104190486
ABS_TOL_ROUNDED = 5e-6
REL_TOL_FULL = 2e-9


@dataclass(frozen=True)
class ArtifactSpec:
    evidence_id: str
    zip_path: Path
    csv_member: str
    total_col: str
    query_col: str


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_csv_from_zip(path: Path, member: str) -> list[dict[str, str]]:
    with zipfile.ZipFile(path) as zf:
        names = set(zf.namelist())
        if member not in names:
            raise AssertionError(f"{path}: missing {member}")
        with zf.open(member) as raw:
            text = io.TextIOWrapper(raw, encoding="utf-8", newline="")
            return list(csv.DictReader(text))


def read_json_members(path: Path, suffix: str) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    with zipfile.ZipFile(path) as zf:
        for name in zf.namelist():
            if name.endswith(suffix):
                with zf.open(name) as f:
                    out.append(json.load(f))
    return out


def geomean(xs: list[float]) -> float:
    if not xs or any(x <= 0 for x in xs):
        raise AssertionError("geomean requires nonempty positive values")
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def log_t_ci95(xs: list[float]) -> tuple[float, float]:
    if len(xs) != 24:
        raise AssertionError(f"frozen verifier expects 24 cells, got {len(xs)}")
    logs = [math.log(x) for x in xs]
    mean = sum(logs) / len(logs)
    variance = sum((x - mean) ** 2 for x in logs) / (len(logs) - 1)
    se = math.sqrt(variance / len(logs))
    return (
        math.exp(mean - T975_DF23 * se),
        math.exp(mean + T975_DF23 * se),
    )


def close(actual: float, expected: float) -> bool:
    # PR62/PR66 headline values in PR87 are intentionally rounded to six decimals;
    # PR80 stores full precision. This tolerance accepts the former while remaining
    # far tighter than any scientifically meaningful change in these ratios.
    return math.isclose(actual, expected, rel_tol=REL_TOL_FULL, abs_tol=ABS_TOL_ROUNDED)


def assert_close(label: str, actual: float, expected: float) -> None:
    if not close(actual, expected):
        raise AssertionError(f"{label}: recomputed={actual!r}, manifest={expected!r}")


def summarize(rows: list[dict[str, str]], total_col: str, query_col: str) -> dict[str, Any]:
    if len(rows) != 24:
        raise AssertionError(f"expected 24 rows, got {len(rows)}")
    exact = sum(r.get("exactness") == "PASS" for r in rows)
    totals = [float(r[total_col]) for r in rows]
    queries = [float(r[query_col]) for r in rows]
    return {
        "cells": len(rows),
        "exact_cells": exact,
        "total_geomean": geomean(totals),
        "total_ci95": list(log_t_ci95(totals)),
        "total_loss_cells": sum(x <= 1.0 for x in totals),
        "minimum_total_cell_ratio": min(totals),
        "query_geomean": geomean(queries),
        "query_ci95": list(log_t_ci95(queries)),
        "query_loss_cells": sum(x <= 1.0 for x in queries),
        "minimum_query_cell_ratio": min(queries),
    }


def verify_core(evidence: dict[str, Any], result: dict[str, Any]) -> None:
    if result["cells"] != evidence["cells"]:
        raise AssertionError("cell-count mismatch")
    if result["exact_cells"] != evidence["exact_cells"]:
        raise AssertionError("exact-cell mismatch")
    if result["total_loss_cells"] != evidence["total_loss_cells"]:
        raise AssertionError("total-loss mismatch")
    if result["query_loss_cells"] != evidence["query_loss_cells"]:
        raise AssertionError("query-loss mismatch")
    assert_close("total geomean", result["total_geomean"], evidence["total_ratio_comparator_over_ades_geomean"])
    assert_close("query geomean", result["query_geomean"], evidence["query_ratio_comparator_over_ades_geomean"])
    assert_close("minimum total ratio", result["minimum_total_cell_ratio"], evidence["minimum_total_cell_ratio"])
    assert_close("minimum query ratio", result["minimum_query_cell_ratio"], evidence["minimum_query_cell_ratio"])

    expected_total_ci = evidence.get("total_ci95", evidence.get("total_ci95_slice_robustness"))
    expected_query_ci = evidence.get("query_ci95", evidence.get("query_ci95_slice_robustness"))
    for i, (actual, expected) in enumerate(zip(result["total_ci95"], expected_total_ci, strict=True)):
        assert_close(f"total ci95[{i}]", actual, float(expected))
    for i, (actual, expected) in enumerate(zip(result["query_ci95"], expected_query_ci, strict=True)):
        assert_close(f"query ci95[{i}]", actual, float(expected))


def verify_pr62(zip_path: Path, evidence: dict[str, Any]) -> dict[str, Any]:
    workloads = read_json_members(zip_path, "/workload.json")
    locality = sorted({int(w["config"]["locality_percent"]) for w in workloads})
    families = sorted({w["config"]["source_family"] for w in workloads})
    if len(workloads) != 24 or locality != [0]:
        raise AssertionError(f"PR62 correction precondition mismatch: workloads={len(workloads)} locality={locality}")
    if "source-policy breadth" not in evidence["mandatory_correction"]:
        raise AssertionError("PR62 manifest lost mandatory source-breadth correction")
    return {"locality_percent_values": locality, "declared_families": families}


def verify_pr66(rows: list[dict[str, str]], evidence: dict[str, Any]) -> dict[str, Any]:
    family_hits: dict[str, int] = {}
    for row in rows:
        family_hits[row["family"]] = family_hits.get(row["family"], 0) + int(row["velo_cache_hits"])
    hot_pool_hits = family_hits.get("hot-pool-2", 0) + family_hits.get("hot-pool-4", 0)
    rotating_hits = family_hits.get("rotating-hot-4", 0)
    if hot_pool_hits != evidence["hot_pool_cache_hits"]:
        raise AssertionError(f"PR66 hot-pool hits {hot_pool_hits} != {evidence['hot_pool_cache_hits']}")
    if rotating_hits != evidence["rotating_hot_cache_hits"]:
        raise AssertionError(f"PR66 rotating hits {rotating_hits} != {evidence['rotating_hot_cache_hits']}")
    peak_v = max(int(row["velo_peak_accounted_state_bytes"]) for row in rows)
    if peak_v > evidence["persistent_state_budget_bytes"]:
        raise AssertionError("PR66 VeloGraphX exceeds frozen logical-state cap")
    return {"family_cache_hits": family_hits, "velographx_peak_accounted_state_bytes": peak_v}


def verify_pr80(rows: list[dict[str, str]], evidence: dict[str, Any]) -> dict[str, Any]:
    hits = sum(int(row["velo_cache_hits"]) for row in rows)
    misses = sum(int(row["velo_cache_misses"]) for row in rows)
    evictions = sum(int(row["velo_cache_evictions"]) for row in rows)
    peak_v = max(int(row["velo_peak_accounted_state_bytes"]) for row in rows)
    peak_a = max(int(row["ades_peak_accounted_state_bytes"]) for row in rows)
    expected = (
        evidence["velographx_cache_hits"],
        evidence["velographx_cache_misses"],
        evidence["velographx_cache_evictions"],
    )
    if (hits, misses, evictions) != expected:
        raise AssertionError(f"PR80 cache totals {(hits, misses, evictions)} != {expected}")
    if peak_v != evidence["velographx_peak_accounted_state_bytes"] or peak_a != evidence["ades_peak_accounted_state_bytes"]:
        raise AssertionError("PR80 peak accounted-state mismatch")
    if peak_v > evidence["persistent_state_budget_bytes"] or peak_a > evidence["persistent_state_budget_bytes"]:
        raise AssertionError("PR80 frozen logical-state cap exceeded")
    if evidence["uncertainty_interpretation"] != "SLICE_LEVEL_ROBUSTNESS_NOT_POPULATION_INFERENCE":
        raise AssertionError("PR80 uncertainty boundary changed")
    graph_seeds = sorted({int(row["graph_seed"]) for row in rows})
    workload_seeds = sorted({int(row["workload_seed"]) for row in rows})
    families = sorted({row["family"] for row in rows})
    if graph_seeds != sorted(evidence["graph_seeds"]) or workload_seeds != sorted(evidence["workload_seeds"]):
        raise AssertionError("PR80 seed-set mismatch")
    if families != sorted(evidence["source_regimes"]):
        raise AssertionError("PR80 source-regime mismatch")
    return {
        "velographx_cache_hits": hits,
        "velographx_cache_misses": misses,
        "velographx_cache_evictions": evictions,
        "velographx_peak_accounted_state_bytes": peak_v,
        "ades_peak_accounted_state_bytes": peak_a,
        "graph_seeds": graph_seeds,
        "workload_seeds": workload_seeds,
        "source_regimes": families,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--pr62", required=True, type=Path)
    parser.add_argument("--pr66", required=True, type=Path)
    parser.add_argument("--pr80", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text())
    if manifest.get("schema") != "ades-pr87-publication-manifest-v1":
        raise AssertionError("unexpected publication manifest schema")
    evidence = {item["id"]: item for item in manifest["central_evidence"]}
    specs = [
        ArtifactSpec("E1_PR62_BOOST_NY", args.pr62, "cells.csv", "speedup_boost_over_ades", "query_speedup_boost_over_ades"),
        ArtifactSpec("E2_PR66_VELOGRAPHX_NY_SOURCE_REUSE", args.pr66, "cells.csv", "speedup_velo_over_ades", "query_speedup_velo_over_ades"),
        ArtifactSpec("E3_PR80_VELOGRAPHX_NONROAD", args.pr80, "results/cells.csv", "speedup_velo_over_ades", "query_speedup_velo_over_ades"),
    ]

    report: dict[str, Any] = {
        "schema": "ades-pr88-independent-artifact-reproduction-v1",
        "status": "PASS",
        "method": "fresh ZIP SHA-256 verification plus independent per-cell CSV recomputation; archived holdout_summary.json is not used",
        "frozen_candidate": manifest["frozen_candidate"],
        "artifacts": {},
    }

    for spec in specs:
        ev = evidence[spec.evidence_id]
        digest = sha256_file(spec.zip_path)
        expected_digest = ev.get("artifact_sha256", ev.get("artifact_digest", "")).removeprefix("sha256:")
        if digest != expected_digest:
            raise AssertionError(f"{spec.evidence_id} ZIP digest {digest} != {expected_digest}")
        rows = read_csv_from_zip(spec.zip_path, spec.csv_member)
        result = summarize(rows, spec.total_col, spec.query_col)
        verify_core(ev, result)
        extra: dict[str, Any] = {}
        if spec.evidence_id.startswith("E1_"):
            extra = verify_pr62(spec.zip_path, ev)
        elif spec.evidence_id.startswith("E2_"):
            extra = verify_pr66(rows, ev)
        elif spec.evidence_id.startswith("E3_"):
            extra = verify_pr80(rows, ev)
        report["artifacts"][spec.evidence_id] = {
            "zip_sha256": digest,
            "csv_member": spec.csv_member,
            **result,
            **extra,
        }

    if manifest["performance_decision"] != "GO_SCOPED_PRACTICAL_SOTA":
        raise AssertionError("unexpected performance decision")
    if manifest["broad_algorithmic_novelty_decision"] != "NO_GO":
        raise AssertionError("reproduction must not promote broad algorithmic novelty")
    report["claim_boundary"] = {
        "performance": "SCOPED_PRACTICAL_SOTA_EVIDENCE_REPRODUCED",
        "broad_algorithmic_novelty": "NO_GO_UNCHANGED",
        "population_inference": "NOT_ESTABLISHED",
        "universal_or_theoretical_sota": "NOT_ESTABLISHED",
    }

    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
