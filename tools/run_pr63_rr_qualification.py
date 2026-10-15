#!/usr/bin/env python3
import argparse
import json
import pathlib
import subprocess
import sys

PIN = "20aa865a52b67bd0ff402e2946e51541db0b5473"
CASES = [
    "simple",
    "zero_equal",
    "asymmetric",
    "nonzero_source",
    "large_cost",
    "random_mixed",
    "parallel_identity",
]
CORE = {
    "simple",
    "zero_equal",
    "asymmetric",
    "nonzero_source",
    "large_cost",
    "random_mixed",
}


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--probe", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--timeout", type=float, default=10.0)
    return p.parse_args()


def main():
    a = parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for case in CASES:
        try:
            p = subprocess.run(
                [a.probe, case], text=True, capture_output=True, timeout=a.timeout
            )
            status = "PASS" if p.returncode == 0 else "MISMATCH"
            result = {
                "case": case,
                "status": status,
                "returncode": p.returncode,
                "stdout": p.stdout,
                "stderr": p.stderr,
            }
        except subprocess.TimeoutExpired as exc:
            result = {
                "case": case,
                "status": "TIMEOUT",
                "returncode": None,
                "stdout": exc.stdout or "",
                "stderr": exc.stderr or "",
            }
        results.append(result)
        (out / f"{case}.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    by_case = {r["case"]: r for r in results}
    core_pass = all(by_case[c]["status"] == "PASS" for c in CORE)
    parallel_pass = by_case["parallel_identity"]["status"] == "PASS"

    if not core_pass:
        classification = "INELIGIBLE_CORE_SEMANTICS"
    elif not parallel_pass:
        classification = "INELIGIBLE_GENERAL_EDGE_ID_WEIGHT_CHANGE"
    else:
        classification = "PENDING_ARBITRARY_SOURCE_MEMORY_TIMING_QUALIFICATION"

    summary = {
        "schema": "ades-pr63-ramalingam-reps-semantic-qualification-v1",
        "upstream_repository": "felipe31/Ramalingam-Reps",
        "upstream_commit": PIN,
        "upstream_algorithm_modified": False,
        "weight_change_adapter": "rr_remove_edge(tail,head) followed by rr_add_edge(tail,head,new_weight)",
        "cases": results,
        "core_semantics_pass": core_pass,
        "parallel_edge_identity_pass": parallel_pass,
        "classification": classification,
        "source_audit": {
            "directed_representation": True,
            "weight_storage": "signed int",
            "distance_storage": "signed int with INF=INT_MAX-500",
            "native_weight_change_api": False,
            "remove_edge_identity": "tail/head endpoint pair only",
            "parallel_insertion_permitted": True,
            "single_source_state": True,
            "arbitrary_source_initialization_possible": True,
            "priority_comparator": "memcmp over int cost bytes",
            "license": "repository MIT; vendored OLSR heap files carry BSD-style license headers",
        },
        "claim_boundary": "No performance comparison is permitted unless semantic DIRECT gates survive. A failure is preserved as comparator evidence and must not be patched away while retaining the same external-comparator identity.",
    }
    (out / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"RR qualification harness failure: {exc}", file=sys.stderr)
        raise
