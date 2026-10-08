#!/usr/bin/env python3
"""Validate the PR48 external-comparator qualification registry."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

SCHEMA = "ades-comparator-registry-v1"
STATUSES = {"DIRECT", "PENDING", "CONTEXT", "INELIGIBLE"}
DIRECT_PASS = "PASS"
REQUIRED_HARD_GATES = (
    "exact_answers",
    "online_operations",
    "directed_graphs",
    "nonnegative_weighted_graphs",
    "weight_increases",
    "weight_decreases",
    "no_future_knowledge",
    "arbitrary_query_sources_supported",
    "matched_trace_adapter",
    "independent_oracle_validation",
    "persistent_memory_budget_enforceable",
    "persistent_memory_accounting_auditable",
    "preprocessing_semantics_declared",
    "timing_boundary_declared",
    "implementation_pinned",
    "license_compatible_for_reproduction",
)
REQUIRED_FIELDS = (
    "id",
    "name",
    "kind",
    "status",
    "implementation",
    "references",
    "declared_semantics",
    "gates",
)


def fail(message: str) -> None:
    raise SystemExit(f"comparator registry validation failed: {message}")


def nonempty_text(value) -> bool:
    return isinstance(value, str) and bool(value.strip())


def validate_reference(comparator_id: str, ref: dict) -> None:
    if not isinstance(ref, dict):
        fail(f"{comparator_id}: reference must be an object")
    if not nonempty_text(ref.get("type")):
        fail(f"{comparator_id}: reference missing type")
    if not (nonempty_text(ref.get("url")) or nonempty_text(ref.get("doi"))):
        fail(f"{comparator_id}: reference requires url or doi")


def validate_implementation(comparator: dict) -> None:
    cid = comparator["id"]
    implementation = comparator["implementation"]
    if not isinstance(implementation, dict):
        fail(f"{cid}: implementation must be an object")
    pinned = implementation.get("pinned")
    if not isinstance(pinned, bool):
        fail(f"{cid}: implementation.pinned must be boolean")
    if pinned:
        if not nonempty_text(implementation.get("repository")):
            fail(f"{cid}: pinned implementation requires repository")
        commit = implementation.get("commit")
        if not nonempty_text(commit) or len(commit) != 40:
            fail(f"{cid}: pinned implementation requires 40-character commit SHA")
        if not nonempty_text(implementation.get("primary_path")):
            fail(f"{cid}: pinned implementation requires primary_path")
        if not nonempty_text(implementation.get("license")):
            fail(f"{cid}: pinned implementation requires license")


def validate_direct(comparator: dict) -> None:
    cid = comparator["id"]
    implementation = comparator["implementation"]
    if not implementation.get("pinned"):
        fail(f"{cid}: DIRECT comparator must pin an executable implementation")
    bad = {gate: comparator["gates"][gate] for gate in REQUIRED_HARD_GATES
           if comparator["gates"][gate] != DIRECT_PASS}
    if bad:
        fail(f"{cid}: DIRECT comparator has unresolved hard gates: {bad}")
    if comparator.get("blockers"):
        fail(f"{cid}: DIRECT comparator cannot retain blockers")
    if comparator.get("mismatches"):
        fail(f"{cid}: DIRECT comparator cannot retain assumption mismatches")


def validate_non_direct(comparator: dict) -> None:
    cid = comparator["id"]
    status = comparator["status"]
    if status == "PENDING":
        blockers = comparator.get("blockers")
        if not isinstance(blockers, list) or not blockers or not all(nonempty_text(x) for x in blockers):
            fail(f"{cid}: PENDING comparator requires nonempty blockers")
    elif status in {"CONTEXT", "INELIGIBLE"}:
        mismatches = comparator.get("mismatches")
        if not isinstance(mismatches, list) or not mismatches or not all(nonempty_text(x) for x in mismatches):
            fail(f"{cid}: {status} comparator requires nonempty mismatches")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", default="benchmarks/comparators/registry.json")
    parser.add_argument("--output")
    args = parser.parse_args()

    registry = json.loads(Path(args.registry).read_text())
    if registry.get("schema") != SCHEMA:
        fail(f"schema must be {SCHEMA}")
    if registry.get("contract_version") != "1.0":
        fail("comparator registry must bind publication contract 1.0")
    if registry.get("claim_ids") != ["C4", "C7", "C-SOTA"]:
        fail("claim_ids must be exactly C4,C7,C-SOTA for PR48")
    if set(registry.get("status_classes", {})) != STATUSES:
        fail("status_classes must define DIRECT/PENDING/CONTEXT/INELIGIBLE")
    if tuple(registry.get("hard_gates", [])) != REQUIRED_HARD_GATES:
        fail("hard_gates do not match the frozen PR48 gate order")

    comparators = registry.get("comparators")
    if not isinstance(comparators, list) or len(comparators) < 4:
        fail("registry must contain at least four reviewed external candidates")

    ids = set()
    direct = []
    pending = []
    context = []
    ineligible = []
    pinned_implementations = []
    literature = []

    for comparator in comparators:
        if not isinstance(comparator, dict):
            fail("comparator entries must be objects")
        for field in REQUIRED_FIELDS:
            if field not in comparator:
                fail(f"entry missing required field {field}")
        cid = comparator["id"]
        if not nonempty_text(cid):
            fail("comparator id must be nonempty")
        if cid in ids:
            fail(f"duplicate comparator id {cid}")
        ids.add(cid)
        if comparator["status"] not in STATUSES:
            fail(f"{cid}: invalid status {comparator['status']}")
        if not nonempty_text(comparator["name"]) or not nonempty_text(comparator["kind"]):
            fail(f"{cid}: name/kind must be nonempty")

        semantics = comparator["declared_semantics"]
        if not isinstance(semantics, dict):
            fail(f"{cid}: declared_semantics must be object")
        for field in ("algorithm_family", "source_model", "query_output", "update_api", "notes"):
            if not nonempty_text(semantics.get(field)):
                fail(f"{cid}: declared_semantics.{field} missing")

        refs = comparator["references"]
        if not isinstance(refs, list) or not refs:
            fail(f"{cid}: at least one reference is required")
        for ref in refs:
            validate_reference(cid, ref)

        gates = comparator["gates"]
        if not isinstance(gates, dict) or set(gates) != set(REQUIRED_HARD_GATES):
            fail(f"{cid}: gates must contain exactly the frozen hard gates")
        if not all(nonempty_text(value) for value in gates.values()):
            fail(f"{cid}: gate values must be nonempty strings")

        validate_implementation(comparator)
        if comparator["implementation"]["pinned"]:
            pinned_implementations.append(cid)
        if comparator["kind"] == "literature_algorithm":
            literature.append(cid)

        status = comparator["status"]
        if status == "DIRECT":
            validate_direct(comparator)
            direct.append(cid)
        else:
            validate_non_direct(comparator)
            {"PENDING": pending, "CONTEXT": context, "INELIGIBLE": ineligible}[status].append(cid)

    if not pinned_implementations:
        fail("at least one real external implementation must be reviewed and pinned")
    if not literature:
        fail("at least one literature-only dynamic shortest-path candidate must be reviewed")
    if not pending:
        fail("PR48 must preserve at least one actionable external qualification candidate")
    if not context:
        fail("PR48 must demonstrate assumption segregation with at least one CONTEXT comparator")

    summary = {
        "schema": "ades-pr48-comparator-qualification-summary-v1",
        "status": "PASS",
        "claim_ids": registry["claim_ids"],
        "reviewed_external_comparators": len(comparators),
        "direct_external_comparators": direct,
        "pending_external_comparators": pending,
        "context_external_comparators": context,
        "ineligible_external_comparators": ineligible,
        "pinned_external_implementations": pinned_implementations,
        "headline_sota_ready": bool(direct),
        "headline_sota_blocker": None if direct else (
            "No external comparator has yet satisfied every PR48 DIRECT hard gate. "
            "C-SOTA therefore remains UNESTABLISHED; PENDING/CONTEXT methods may not be presented as direct wins."
        ),
        "direct_rule": "Every frozen hard gate must equal PASS; theory-only or contextual evidence is insufficient.",
    }
    text = json.dumps(summary, indent=2) + "\n"
    if args.output:
        Path(args.output).write_text(text)
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
