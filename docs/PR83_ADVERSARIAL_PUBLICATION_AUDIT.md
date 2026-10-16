# PR83 — Adversarial publication and reproducibility audit

## Status

**CLAIM-HOSTILE AUDIT. No performance tuning.**

PR83 attempts to falsify the PR81 `GO — SCOPED PRACTICAL SOTA` decision after PR82 closed the comparator-registry bookkeeping gap. It does not search for additional favorable cells and does not alter ADES, VeloGraphX, Boost.Graph, benchmark seeds, timing semantics, memory semantics, or Publication Contract v1.0.

## Claims addressed

```text
claim_ids: [C1, C2, C3, C4, C-SOTA]
```

## Audit question

> Does the archived PR41→PR82 evidence package support the **bounded** C-SOTA decision without laundering comparator assumptions, statistical dependence, memory definitions, historical bookkeeping, or empirical performance into broader claims than the evidence warrants?

## Falsification targets

PR83 treats any of the following as a publication-level defect:

1. fewer than two pinned DIRECT external implementations after PR82;
2. a DIRECT comparator with any frozen hard gate other than exact `PASS`;
3. wording that presents the VeloGraphX adapter as native upstream arbitrary-source behavior;
4. wording that presents the 64 MiB fairness rule as equal actual memory or equal RSS;
5. population-level statistical claims inferred from PR80's nested 3-graph × 4-family × 2-workload slice;
6. counting assumption-mismatched NetworKit or fixed-source GraphBolt evidence as unrestricted DIRECT evidence for full C-SOTA;
7. converting practical benchmark superiority into a claim of theoretical/asymptotic novelty;
8. deleting or rewriting the historical PR81 manifest merely because PR82 later closed its pending registry action;
9. universal “beats all algorithms / world's fastest” wording;
10. irreproducible evidence identifiers or silent changes to the frozen contract.

## Expected evidence surfaces

The audit consumes, rather than edits:

- `docs/PUBLICATION_CONTRACT.md`;
- `docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md`;
- `evidence/pr81_scoped_sota_decision.json`;
- `benchmarks/comparators/registry.json` after PR82;
- `docs/PR82_VELOGRAPHX_REGISTRY_RECONCILIATION.md`;
- `README.md`;
- frozen adapter/pair-driver identities from PR65/82.

The executable audit is `tools/audit_pr83_publication_package.py`.

## Predeclared dispositions

### `PASS_SCOPED_SOTA_PACKAGE_WITH_MANDATORY_BOUNDARIES`

Use only if no finding invalidates C-SOTA and every critical boundary remains explicit. This is **not** an upgrade to universal SOTA. It means PR81 survives adversarial interpretation under its original frozen scope.

### `FAIL_SCOPED_SOTA_PACKAGE`

Use if any audit finding invalidates the bounded decision, including comparator-gate failure, hidden assumption mismatch, contract drift, unsupported statistical generalization, or irreproducible central evidence.

A failure is accepted evidence and must not be repaired by changing the claim contract after seeing the audit.

## Mandatory boundaries even on PASS

A passing package must still say all of the following, in substance:

- the result is **scoped practical SOTA evidence**, not universal or theoretical SOTA;
- VeloGraphX arbitrary-source comparison is **adapter-mediated**;
- the 64 MiB rule is a common persistent-state **cap/accounting contract**, not equal actual bytes or equal RSS;
- PR80 confidence intervals characterize robustness of the preregistered nested slice, not a population of all graphs/workloads;
- NetworKit and historical-Cilk GraphBolt remain contextual for the complete frozen C-SOTA scope;
- empirical SOTA evidence does not establish algorithmic novelty or a new asymptotic bound.

## Known issue admitted before execution

The repository README remains conservative but stale: it still says publication-grade evidence should rerun a complete fixed-commit, multi-seed matrix. PR62, PR66, and PR80 subsequently supplied fresh fixed-candidate holdouts. This is predeclared as a documentation-drift candidate, not an evidence-invalidating failure.

The audit must report it rather than silently correcting the README before the claim-hostile pass.

## Acceptance rule

PR83 is accepted only if:

1. the audit script runs without modifying any performance source;
2. current registry validation still passes independently;
3. PR82 has both Boost.Graph and VeloGraphX in DIRECT;
4. all mandatory claim boundaries are emitted into the audit artifact;
5. every non-PASS finding is retained visibly;
6. the workflow accepts `FAIL_SCOPED_SOTA_PACKAGE` as a scientifically valid completed audit outcome, while integrity/runtime errors remain hard CI failures.

If PR83 passes, the next repair should address only publication-surface drift and other audit findings. It must not use the audit as permission to expand the frozen claim.