# PR84 — Public evidence-status repair

## Status

**PUBLICATION-SURFACE REPAIR AUTHORIZED BY PR83 FINDING H1.**

PR84 updates the public README after the adversarial PR83 audit identified one non-invalidating documentation drift: the README still asked for a fixed-commit, multi-seed publication rerun even though PR62, PR66, and PR80 had subsequently supplied fresh frozen-candidate holdouts.

PR84 does not change ADES code, comparator code, benchmark inputs, seeds, statistics, Publication Contract v1.0, the PR81 decision, or the PR82 comparator registry.

Because the repaired README is itself an input to the inherited PR83 audit, PR84 also makes one audit-harness state transition explicit: H1 may move from `REPAIR_RECOMMENDED` to `CLOSED_BY_PR84` when, and only when, the obsolete sentence is absent **and** the repaired README contains the mandatory evidence/boundary markers. This does not weaken any scientific criterion; malformed or boundary-losing repairs still produce `FAIL`.

## Claims addressed

```text
claim_ids: [C1, C2, C3, C4, C-SOTA]
```

## Triggering evidence

Canonical repaired PR83 audit before PR84:

- head: `b4cff3a5c1220ea6922ac72c62986bd3544a7a75`;
- workflow run: `38859960130`;
- job: `116839760404`;
- artifact: `12028887027`;
- artifact digest: `sha256:8d33bd7c4f68a04cf8d449ff3ad9b625601a068d508bcdc69de3207f85864156`;
- disposition: `PASS_SCOPED_SOTA_PACKAGE_WITH_MANDATORY_BOUNDARIES`;
- invalidating findings: `0`;
- repair finding: `H1 — README evidence-status section is stale`;
- open boundary retained: `G1 — empirical performance evidence does not establish scholarly novelty or a new asymptotic bound`.

## Repair contract

The README update must:

1. remove the obsolete statement that publication-grade evidence still needs fixed-commit multi-seed reruns;
2. state only **scoped practical SOTA evidence** under the frozen exact-online benchmark contract;
3. preserve the statement that scholarly novelty is not established;
4. preserve the prohibition on universal/theoretical SOTA wording;
5. identify Boost.Graph recomputation and adapter-mediated VeloGraphX as the two DIRECT comparator classes;
6. describe VeloGraphX arbitrary-source behavior as adapter-mediated, not native upstream behavior;
7. describe 64 MiB as a common auditable persistent-state cap/accounting contract, not equal actual memory or RSS;
8. describe PR80 confidence intervals as slice-level robustness rather than population-level graph inference;
9. keep NetworKit and historical-Cilk GraphBolt assumption-separated/contextual for full C-SOTA;
10. link the public evidence status to PR81, PR82, and PR83 documentation;
11. change no solver, benchmark, comparator, or historical evidence file.

The inherited PR83 audit transition must additionally:

12. recognize `H1=REPAIR_RECOMMENDED` while the original stale sentence remains;
13. recognize `H1=CLOSED_BY_PR84` only when the obsolete sentence is gone and PR62/66/80 plus the scoped/novelty/adapter/statistical boundary markers are present;
14. retain `H1=FAIL` if neither the original conservative caveat nor a compliant repair is present;
15. keep `G1=OPEN_BOUNDARY` after H1 closes.

## Nonclaims

PR84 does not establish:

- universal dynamic-shortest-path SOTA;
- theoretical/asymptotic SOTA;
- scholarly novelty of ADES mechanisms;
- superiority over every published implementation;
- equal process RSS;
- native arbitrary-source VeloGraphX semantics;
- population-level performance over arbitrary graphs/workload distributions.

## Acceptance rule

PR84 is accepted only if its CI guard and the inherited adversarial audit verify every repair-contract item above and the PR diff remains confined to publication/audit surfaces:

- `README.md`;
- this repair document;
- `tools/audit_pr83_publication_package.py` only for the H1 open→closed state transition;
- `.github/workflows/pr83-adversarial-publication-audit.yml` only to accept/verify that state transition on descendant publication repairs;
- `.github/workflows/pr84-public-evidence-status-repair.yml`.

No solver, comparator, benchmark, statistical evidence, registry, or historical evidence artifact may change.

If the public README cannot state the new evidence status without violating a PR83 mandatory boundary, the repair must fail rather than weakening the boundary.