#!/usr/bin/env python3
"""Adversarially audit the PR81/PR82 scoped-SOTA evidence package.

The audit is claim-hostile. Scientific defects are emitted as FAIL findings and
produce a completed FAIL_SCOPED_SOTA_PACKAGE disposition; malformed/missing
inputs remain hard runtime failures. No performance result is recomputed here.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_json(path: str):
    return json.loads((ROOT / path).read_text())


def read(path: str) -> str:
    return (ROOT / path).read_text()


def finding(fid: str, severity: str, status: str, statement: str, evidence: str) -> dict:
    return {"id": fid, "severity": severity, "status": status,
            "statement": statement, "evidence": evidence}


def main() -> int:
    registry = load_json("benchmarks/comparators/registry.json")
    decision = load_json("evidence/pr81_scoped_sota_decision.json")
    contract = read("docs/PUBLICATION_CONTRACT.md")
    pr81 = read("docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md")
    pr82 = read("docs/PR82_VELOGRAPHX_REGISTRY_RECONCILIATION.md")
    readme = read("README.md")

    findings: list[dict] = []

    # A — frozen claim boundary.
    a_ok = (
        "**Contract version:** 1.0" in contract
        and "universal dynamic-shortest-path SOTA" in contract
        and "C-SOTA" in contract
        and decision.get("publication_contract_version") == "1.0"
        and decision.get("decision") == "GO_SCOPED_PRACTICAL_SOTA"
    )
    findings.append(finding(
        "A1", "CRITICAL", "PASS" if a_ok else "FAIL",
        "The headline decision remains bound to Publication Contract v1.0 rather than a universal SOTA claim.",
        "Publication contract and PR81 machine-readable decision must agree on scoped C-SOTA.",
    ))

    # B — DIRECT comparator set and all frozen hard gates.
    direct = {c["id"]: c for c in registry["comparators"] if c["status"] == "DIRECT"}
    required_direct = {"boost-graph-dijkstra-recompute", "velographx-weighted-sssp-adapter"}
    b_ok = required_direct <= set(direct)
    if b_ok:
        for cid in required_direct:
            c = direct[cid]
            b_ok = b_ok and c["implementation"].get("pinned") is True
            b_ok = b_ok and all(c["gates"].get(g) == "PASS" for g in registry["hard_gates"])
    findings.append(finding(
        "B1", "CRITICAL", "PASS" if b_ok else "FAIL",
        "The qualified exact-online comparator set contains two pinned DIRECT implementations and both satisfy all frozen hard gates.",
        "Boost.Graph recomputation and adapter-mediated VeloGraphX must remain DIRECT under the unchanged registry rules.",
    ))

    # C — VeloGraphX arbitrary-source semantics must remain explicitly adapter-mediated.
    velo = direct.get("velographx-weighted-sssp-adapter", {})
    semantics_text = json.dumps(velo.get("declared_semantics", {}), sort_keys=True).lower()
    pr81_lower = pr81.lower()
    pr82_lower = pr82.lower()
    c_ok = (
        "adapter" in semantics_text
        and ("fixed source" in semantics_text or "fixed-source" in semantics_text)
        and "adapter-mediated" in pr82_lower
        and "upstream velographx natively provides arbitrary-source queries" in pr82_lower
        and "does not mean" in pr82_lower
        and "natively at arbitrary-source semantics" in pr81_lower
        and "transparent byte-bounded adapter" in pr81_lower
    )
    findings.append(finding(
        "C1", "HIGH", "PASS_WITH_BOUNDARY" if c_ok else "FAIL",
        "VeloGraphX is a valid DIRECT comparator only through the transparent bounded arbitrary-source adapter; upstream native arbitrary-source semantics are not claimed.",
        "PR65/PR82 qualification text and PR81 prohibited-wording section must preserve this distinction.",
    ))

    # Locate PR80 evidence once for memory/statistical checks.
    velo81 = next(c for c in decision["direct_comparator_evidence"]
                  if c["id"] == "velographx-weighted-sssp-adapter")
    pr80 = next(e for e in velo81["evidence"] if e["campaign"] == "PR80")
    budget = velo81["logical_state_budget_bytes"]

    # D — memory fairness means common cap/accounting, not equal actual memory/RSS.
    d_ok = (
        pr80["velographx_peak_accounted_state_bytes"] <= budget
        and pr80["ades_peak_accounted_state_bytes"] <= budget
        and pr80["velographx_peak_accounted_state_bytes"] != pr80["ades_peak_accounted_state_bytes"]
        and "equal process RSS" in decision["nonclaims"]
    )
    findings.append(finding(
        "D1", "HIGH", "PASS_WITH_BOUNDARY" if d_ok else "FAIL",
        "Memory fairness means both systems obey the same auditable persistent-state cap; it does not mean equal actual bytes or equal RSS.",
        f"PR80 accounted peaks: VeloGraphX={pr80['velographx_peak_accounted_state_bytes']} bytes, ADES={pr80['ades_peak_accounted_state_bytes']} bytes, cap={budget} bytes.",
    ))

    # E — nested slice robustness must not be promoted to graph-population inference.
    e_ok = (
        pr80["exact_cells"] == 24
        and pr80["total_loss_cells"] == 0
        and pr80["query_loss_cells"] == 0
        and pr80["total_ci95"][0] > 1
        and pr80["query_ci95"][0] > 1
        and "population-level dominance over arbitrary graph/workload distributions" in decision["nonclaims"]
    )
    findings.append(finding(
        "E1", "HIGH", "PASS_WITH_BOUNDARY" if e_ok else "FAIL",
        "The 24 PR80 cells are a preregistered nested slice (3 graph seeds × 4 source families × 2 workload seeds), not 24 independent graph-population samples.",
        "Confidence intervals may be described as robustness evidence for the preregistered slice, not population-level inference over all graphs/workloads.",
    ))

    # F — contextual comparators stay segregated from unrestricted DIRECT evidence.
    contextual = {x["id"] for x in decision["contextual_evidence"]}
    f_ok = (
        "networkit-11.2.2-nonroad" in contextual
        and "graphbolt-historical-cilk-fixed-source" in contextual
        and "networkit-dyndijkstra" not in direct
    )
    findings.append(finding(
        "F1", "HIGH", "PASS" if f_ok else "FAIL",
        "NetworKit and historical-Cilk GraphBolt results remain contextual and are not counted as unrestricted DIRECT wins for full C-SOTA.",
        "PR81 contextual-evidence ledger and PR82 registry must preserve assumption-class separation.",
    ))

    # G — empirical SOTA is not a novelty/asymptotic proof.
    g_ok = (
        "novelty is **not established**" in readme
        and "theoretical or asymptotic SOTA" in decision["nonclaims"]
    )
    findings.append(finding(
        "G1", "HIGH", "OPEN_BOUNDARY" if g_ok else "FAIL",
        "Scoped practical performance-SOTA evidence does not establish scholarly novelty of the underlying repair/residency mechanisms or a new asymptotic bound.",
        "README and PR81 nonclaims must retain novelty/asymptotic separation.",
    ))

    # H — public README is conservative but stale relative to PR62/66/80.
    stale_phrase = (
        "publication-grade evidence should rerun the complete matrix on one fixed commit "
        "across multiple independent workload seeds and longer traces"
    )
    h_ok = stale_phrase in readme
    findings.append(finding(
        "H1", "MEDIUM", "REPAIR_RECOMMENDED" if h_ok else "FAIL",
        "The README evidence-status section predates the completed fresh fixed-candidate multi-seed holdouts and should be updated before public manuscript/release positioning.",
        "The stale sentence is conservative rather than falsely favorable, so it is a documentation-drift repair rather than an invalidation of PR81.",
    ))

    # I — preserve PR81 as a historical snapshot after PR82 closes its registry action.
    i_ok = velo81.get("class") == "DIRECT_PENDING_REGISTRY_RECONCILIATION" and velo.get("status") == "DIRECT"
    findings.append(finding(
        "I1", "LOW", "CLOSED_BY_PR82" if i_ok else "FAIL",
        "PR81's historical manifest records the then-pending registry reconciliation; PR82 closes it in the canonical registry without retroactively rewriting PR81.",
        "Historical evidence snapshots should remain immutable.",
    ))

    # J — even large margins do not license universal claims.
    j_ok = (
        pr80["minimum_total_cell_ratio"] > 1
        and pr80["minimum_query_cell_ratio"] > 1
        and pr80["strong_preregistered_pass"] is True
        and "superiority over every published algorithm" in decision["nonclaims"]
    )
    findings.append(finding(
        "J1", "CRITICAL", "PASS_WITH_BOUNDARY" if j_ok else "FAIL",
        "Large observed margins do not license claims about unqualified/unexecuted algorithms or all graph distributions.",
        "PR80 survives its preregistered slice; PR81 must continue to forbid superiority-over-every-algorithm wording.",
    ))

    invalidating = [f for f in findings if f["status"] in {"FAIL", "INVALIDATES_C_SOTA"}]
    disposition = "FAIL_SCOPED_SOTA_PACKAGE" if invalidating else "PASS_SCOPED_SOTA_PACKAGE_WITH_MANDATORY_BOUNDARIES"
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
        "invalidating_findings": invalidating,
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
    return 1 if invalidating else 0


if __name__ == "__main__":
    raise SystemExit(main())
