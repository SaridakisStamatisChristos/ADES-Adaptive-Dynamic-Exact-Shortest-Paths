#!/usr/bin/env python3
"""Adversarially audit the PR81/PR82 scoped-SOTA evidence package.

This tool is intentionally claim-hostile: it does not recompute favorable
performance statistics. It checks whether the evidence package can support the
bounded claim without silently broadening statistical, comparator, memory, or
novelty assumptions.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def load_json(path: str):
    return json.loads((ROOT / path).read_text())


def read(path: str) -> str:
    return (ROOT / path).read_text()


def finding(fid: str, severity: str, status: str, statement: str, evidence: str) -> dict:
    return {
        "id": fid,
        "severity": severity,
        "status": status,
        "statement": statement,
        "evidence": evidence,
    }


def main() -> int:
    registry = load_json("benchmarks/comparators/registry.json")
    decision = load_json("evidence/pr81_scoped_sota_decision.json")
    contract = read("docs/PUBLICATION_CONTRACT.md")
    pr81 = read("docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md")
    pr82 = read("docs/PR82_VELOGRAPHX_REGISTRY_RECONCILIATION.md")
    readme = read("README.md")

    findings: list[dict] = []

    # A. Frozen claim contract must remain bounded and unchanged in meaning.
    assert "**Contract version:** 1.0" in contract
    assert "universal dynamic-shortest-path SOTA" in contract
    assert "C-SOTA" in contract
    assert decision["publication_contract_version"] == "1.0"
    assert decision["decision"] == "GO_SCOPED_PRACTICAL_SOTA"
    findings.append(finding(
        "A1", "CRITICAL", "PASS",
        "The headline decision remains bound to Publication Contract v1.0 rather than a universal SOTA claim.",
        "Publication contract and PR81 machine-readable decision agree on scoped C-SOTA.",
    ))

    # B. Direct comparator set must be real, pinned, and pass all frozen gates.
    direct = {c["id"]: c for c in registry["comparators"] if c["status"] == "DIRECT"}
    required_direct = {"boost-graph-dijkstra-recompute", "velographx-weighted-sssp-adapter"}
    assert required_direct <= set(direct)
    for cid in required_direct:
        c = direct[cid]
        assert c["implementation"]["pinned"] is True
        assert all(c["gates"][g] == "PASS" for g in registry["hard_gates"])
    findings.append(finding(
        "B1", "CRITICAL", "PASS",
        "The qualified exact-online comparator set contains two pinned DIRECT implementations and both satisfy all frozen hard gates.",
        "Boost.Graph recomputation and adapter-mediated VeloGraphX are DIRECT in the PR82 registry.",
    ))

    # C. VeloGraphX source semantics must never be laundered into a native arbitrary-source claim.
    velo = direct["velographx-weighted-sssp-adapter"]
    semantics_text = json.dumps(velo["declared_semantics"], sort_keys=True).lower()
    assert "adapter" in semantics_text
    assert "fixed source" in semantics_text or "fixed-source" in semantics_text
    assert "DIRECT remains adapter-mediated" in pr82
    assert "native arbitrary-source" in pr81
    findings.append(finding(
        "C1", "HIGH", "PASS_WITH_BOUNDARY",
        "VeloGraphX is a valid DIRECT comparator only through the transparent bounded arbitrary-source adapter; upstream native arbitrary-source semantics are not claimed.",
        "PR65/PR82 qualification text and PR81 prohibited-wording section preserve this distinction.",
    ))

    # D. Memory fairness is a matched cap/accounting contract, not equal actual bytes or RSS.
    pr80 = next(
        e for c in decision["direct_comparator_evidence"]
        if c["id"] == "velographx-weighted-sssp-adapter"
        for e in c["evidence"] if e["campaign"] == "PR80"
    )
    budget = next(c for c in decision["direct_comparator_evidence"]
                  if c["id"] == "velographx-weighted-sssp-adapter")["logical_state_budget_bytes"]
    assert pr80["velographx_peak_accounted_state_bytes"] <= budget
    assert pr80["ades_peak_accounted_state_bytes"] <= budget
    assert pr80["velographx_peak_accounted_state_bytes"] != pr80["ades_peak_accounted_state_bytes"]
    assert "equal process RSS" in decision["nonclaims"]
    findings.append(finding(
        "D1", "HIGH", "PASS_WITH_BOUNDARY",
        "Memory fairness means both systems obey the same auditable persistent-state cap; it does not mean equal actual bytes or equal RSS.",
        f"PR80 accounted peaks: VeloGraphX={pr80['velographx_peak_accounted_state_bytes']} bytes, ADES={pr80['ades_peak_accounted_state_bytes']} bytes, cap={budget} bytes.",
    ))

    # E. PR80 statistics must not be promoted to population-level inference.
    assert pr80["exact_cells"] == 24
    assert pr80["total_loss_cells"] == 0 and pr80["query_loss_cells"] == 0
    assert pr80["total_ci95"][0] > 1 and pr80["query_ci95"][0] > 1
    assert "population-level dominance over arbitrary graph/workload distributions" in decision["nonclaims"]
    findings.append(finding(
        "E1", "HIGH", "PASS_WITH_BOUNDARY",
        "The 24 PR80 cells are a preregistered nested slice (3 graph seeds × 4 source families × 2 workload seeds), not 24 independent graph-population samples.",
        "PR81 nonclaims explicitly prohibit population-level dominance; confidence intervals must be described as slice-level robustness evidence.",
    ))

    # F. Context comparators must remain segregated.
    contextual = {x["id"] for x in decision["contextual_evidence"]}
    assert "networkit-11.2.2-nonroad" in contextual
    assert "graphbolt-historical-cilk-fixed-source" in contextual
    assert "networkit-dyndijkstra" not in direct
    findings.append(finding(
        "F1", "HIGH", "PASS",
        "NetworKit and historical-Cilk GraphBolt results remain contextual and are not counted as unrestricted DIRECT wins for full C-SOTA.",
        "PR81 contextual-evidence ledger and PR82 registry preserve assumption-class separation.",
    ))

    # G. Performance SOTA evidence must not be misrepresented as algorithmic novelty.
    assert "novelty is **not established**" in readme
    assert "theoretical or asymptotic SOTA" in decision["nonclaims"]
    findings.append(finding(
        "G1", "HIGH", "OPEN_BOUNDARY",
        "Scoped practical performance-SOTA evidence does not establish scholarly novelty of the underlying repair/residency mechanisms or a new asymptotic bound.",
        "README and PR81 nonclaims retain novelty/asymptotic separation.",
    ))

    # H. README is deliberately conservative but now stale relative to PR62/66/80.
    stale_phrase = "publication-grade evidence should rerun the complete matrix on one fixed commit across multiple independent workload seeds and longer traces"
    assert stale_phrase in readme
    findings.append(finding(
        "H1", "MEDIUM", "REPAIR_RECOMMENDED",
        "The README evidence-status section predates the completed fresh fixed-candidate multi-seed holdouts and should be updated before public manuscript/release positioning.",
        "The stale sentence is conservative rather than falsely favorable, so it does not invalidate PR81; it creates documentation drift.",
    ))

    # I. Historical PR81 manifest intentionally records the then-pending registry action.
    velo81 = next(c for c in decision["direct_comparator_evidence"]
                  if c["id"] == "velographx-weighted-sssp-adapter")
    assert velo81["class"] == "DIRECT_PENDING_REGISTRY_RECONCILIATION"
    assert velo["status"] == "DIRECT"
    findings.append(finding(
        "I1", "LOW", "CLOSED_BY_PR82",
        "PR81's historical manifest still says DIRECT_PENDING_REGISTRY_RECONCILIATION; PR82 has now closed that action in the canonical registry.",
        "Preserve PR81 as an immutable historical snapshot; do not rewrite it retroactively.",
    ))

    # J. Strong numerical evidence is necessary but not sufficient to expand the claim universe.
    assert pr80["minimum_total_cell_ratio"] > 1
    assert pr80["minimum_query_cell_ratio"] > 1
    assert pr80["strong_preregistered_pass"] is True
    assert "superiority over every published algorithm" in decision["nonclaims"]
    findings.append(finding(
        "J1", "CRITICAL", "PASS_WITH_BOUNDARY",
        "Large observed margins do not license claims about unqualified/unexecuted algorithms or all graph distributions.",
        "PR80 survives its preregistered slice; PR81 explicitly forbids superiority-over-every-algorithm wording.",
    ))

    invalidating = [f for f in findings if f["status"] in {"FAIL", "INVALIDATES_C_SOTA"}]
    disposition = (
        "FAIL_SCOPED_SOTA_PACKAGE" if invalidating
        else "PASS_SCOPED_SOTA_PACKAGE_WITH_MANDATORY_BOUNDARIES"
    )
    result = {
        "schema": "ades-pr83-adversarial-publication-audit-v1",
        "disposition": disposition,
        "publication_contract_version": "1.0",
        "audited_decision": "GO_SCOPED_PRACTICAL_SOTA",
        "direct_comparators": sorted(direct),
        "finding_count": len(findings),
        "critical_or_high_findings": [f for f in findings if f["severity"] in {"CRITICAL", "HIGH"}],
        "repair_recommended": [f for f in findings if f["status"] == "REPAIR_RECOMMENDED"],
        "open_boundaries": [f for f in findings if f["status"] == "OPEN_BOUNDARY"],
        "findings": findings,
        "mandatory_manuscript_boundaries": [
            "Use 'scoped practical SOTA evidence' or equivalent, never universal/theoretical SOTA.",
            "Describe VeloGraphX arbitrary-source comparison as adapter-mediated.",
            "Describe 64 MiB as a common persistent-state cap/accounting contract, not equal actual memory or RSS.",
            "Describe PR80 uncertainty as robustness over the preregistered nested slice, not population-level inference.",
            "Keep NetworKit and historical-Cilk GraphBolt in contextual/assumption-separated classes for full C-SOTA.",
            "Do not infer algorithmic novelty or asymptotic improvement from empirical SOTA evidence.",
        ],
        "next_actions": [
            "Update public-facing README evidence status without deleting historical caveats.",
            "Run a separate prior-art/novelty audit if the manuscript will make mechanism-novelty claims.",
            "Prefer a new orthogonal DIRECT comparator or real directed arbitrary-source topology over accumulating more favorable cells.",
        ],
    }
    out = ROOT / "evidence" / "pr83_publication_audit.json"
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not invalidating else 1


if __name__ == "__main__":
    raise SystemExit(main())
