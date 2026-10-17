# ADES Publication Contract

**Contract version:** 1.0  
**Status:** FROZEN for the publication-oriented campaign beginning with PR41  
**Applies to:** publication-facing experiments, analyses, figures, tables, and claims created after this contract is merged

## 1. Purpose

This document freezes the scientific question, benchmark scope, primary metrics, admissible comparative claims, and evidence rules for the ADES publication campaign before further benchmark development.

The campaign is designed to determine whether the current ADES mechanism earns a defensible **scoped state-of-the-art claim**. It is not designed to manufacture a SOTA result. Negative, neutral, and contradictory evidence are first-class outcomes.

## 2. Primary research question

> **RQ1.** Can adaptive bounded residency of exact single-source shortest-path state improve the runtime-memory Pareto frontier for online shortest-path workloads containing arbitrary edge-weight increases/decreases and repeated-source queries, without future knowledge?

## 3. Candidate SOTA scope

The primary claim domain is restricted to workloads satisfying all of the following:

- directed weighted graphs;
- nonnegative edge weights;
- online operation streams;
- edge-weight increases and decreases;
- exact point-to-point shortest-path answers;
- no knowledge of future operations;
- bounded algorithm-owned persistent memory;
- repeated arbitrary query sources;
- adaptive source residency.

A result outside this domain may be reported, but it must not silently broaden the primary claim.

## 4. Frozen claim boundary

The campaign may seek to establish only the following scoped headline claim:

> **C-SOTA.** Within the qualified exact-online comparator set and the frozen benchmark scope, ADES establishes or materially improves the runtime-memory Pareto frontier under at least a defensible subset of mixed-update, repeated-source workloads.

`C-SOTA` is a **candidate claim**, not an established fact. It remains `UNESTABLISHED` until the definitive evidence package supports it.

The initial paper must **not** claim:

- universal dynamic-shortest-path SOTA;
- a universal asymptotic improvement over Dijkstra or established dynamic shortest-path bounds;
- novelty of dynamic SSSP repair by itself;
- superiority to algorithms operating under fundamentally different preprocessing, source, graph, approximation, offline-information, or memory assumptions unless those differences are explicitly separated and the comparison is justified;
- production-scale generality beyond the tested graph/workload domain;
- statistical certainty unsupported by independent workload samples and the declared analysis plan.

## 5. Supporting claim registry

Every publication-facing experiment must map to one or more claim identifiers from this registry. New central claims require an explicit contract revision rather than being introduced post hoc in analysis prose.

| ID | Candidate claim | Evidence status at freeze | Minimum evidence needed |
|---|---|---|---|
| C1 | Exactness is preserved under the supported online query/update semantics. | Existing implementation evidence; publication campaign must revalidate. | Independent oracle agreement on every matched query used for performance evidence. |
| C2 | Compared algorithms in a matched cell receive the same operation stream. | Deterministic generation exists; cryptographic identity pending PR42. | Canonical replay plus identical trace identity and operation counts. |
| C3 | ADES respects the same declared persistent-state byte budget as bounded comparators. | Not yet established. | Common byte-accounting model and hard budget enforcement. |
| C4 | Adaptive bounded residency improves the runtime-memory tradeoff relative to cold exact search and fixed/bounded residency controls in at least some defined regimes. | Partially supported by existing NY engineering evidence; not publication-established. | Multi-graph, repeated, equal-byte comparison with uncertainty estimates. |
| C5 | Work-aware admission improves on frequency-only admission where admission quality matters. | Unestablished. | Controlled ablation with the same underlying code path where practical. |
| C6 | Update-debt-aware residency improves robustness as update pressure rises. | Unestablished. | Controlled update-rate ablation and telemetry explaining the effect. |
| C7 | The full ADES policy materially outperforms a simple frequency + LRU + dynamic-repair policy in at least a defensible operating region. | Unestablished. | Direct scientific ablation; effect larger than measurement noise. |
| C8 | No fixed strategy dominates the complete tested workload space. | Unestablished. | Adversarial workload sweep preserving both wins and losses. |
| C9 | Residency profitability can be explained by avoided cold work minus promotion and maintenance cost. | Hypothesis only. | Mechanism telemetry and predictive/crossover analysis. |
| C-SOTA | ADES establishes or improves the scoped runtime-memory Pareto frontier against qualified exact-online comparators. | UNESTABLISHED. | Definitive campaign satisfying C1-C8 where applicable, qualified comparators, statistics, archived raw evidence, and reproducible analysis. |

## 6. Primary metrics

The following are the primary publication metrics and must retain stable meanings across the campaign.

### Runtime

- `total_algorithm_time`: wall-clock time spent by the algorithm under test for the matched operation stream, excluding oracle construction and validation work.
- `query_time`: aggregate timed query-processing work.
- `update_time`: aggregate timed update-processing work.
- `p50_latency`: median per-operation or declared per-class latency, with the operation class stated explicitly.
- `p95_latency`: 95th-percentile per-operation or declared per-class latency, with the operation class stated explicitly.

Component timings introduced later may refine these totals but must not redefine them silently.

### Memory

- `peak_rss`: process-level peak resident-set size, recorded independently as an observational system metric.
- `accounted_algorithm_state_bytes`: bytes of algorithm-owned persistent state included in the common publication budget model.

`peak_rss` and `accounted_algorithm_state_bytes` are **not interchangeable**. Publication fairness controls should use the common algorithm-state accounting model; RSS remains separately reported.

### Comparative statistics

- geometric mean speedup over explicitly named paired comparator;
- paired win/tie/loss counts;
- 95% confidence intervals using the frozen statistical method for the corresponding campaign stage;
- runtime-memory Pareto dominance or nondominance under matched conditions.

Raw paired values must remain available wherever an aggregate comparative statistic is reported.

## 7. Matched-comparison requirements

A publication-facing matched benchmark cell must identify at minimum:

- graph identity and cryptographic hash;
- trace identity;
- seed and generator configuration, where generated;
- algorithm/variant identifier;
- comparator eligibility class;
- configured persistent-state byte budget, where applicable;
- operation counts and update composition;
- exactness/oracle status;
- repetition/sample identity;
- commit SHA;
- claim IDs addressed by the cell or experiment.

A comparator result is not admissible as direct support for `C-SOTA` when materially different assumptions make the comparison non-equivalent. Such results may instead be reported in a separately labeled context class.

## 8. Experiment-to-claim mapping rule

Every new publication-facing experiment specification, manifest, or archived evidence package created after PR41 must declare:

```text
claim_ids: [C...]
```

or an equivalent machine-readable field.

For human-readable experiment documents, a dedicated **Claims addressed** section is also required.

An experiment with no mapped claim is exploratory only and must not be used later as central confirmatory evidence without being incorporated into a declared protocol revision.

For each mapped claim, the analysis must classify the result as one of:

```text
SUPPORTED
PARTIAL
CONTRADICTED
INCONCLUSIVE
NOT_APPLICABLE
```

Contradicted and inconclusive results must remain in the archived evidence.

## 9. Evidence admissibility rules

Publication-facing comparative evidence is admissible only when all applicable conditions hold:

1. **Exactness:** every measured query used for a central performance claim agrees with the independent exact oracle.
2. **Trace parity:** matched algorithms consume the same operation stream under the trace-identity mechanism current for that campaign stage.
3. **Fair budget:** bounded algorithms obey the declared common persistent-state byte budget once PR43 is in force.
4. **Timing isolation:** oracle construction/validation is excluded from algorithm timing.
5. **Provenance:** graph, trace/configuration, code revision, build/runtime environment, and raw measurements are recoverable.
6. **Comparator qualification:** direct comparators satisfy the documented eligibility criteria or are explicitly segregated by assumption class.
7. **No favorable-cell filtering:** failed hypotheses, losses, and contradictory cells remain visible unless excluded by a predeclared rule.
8. **Reproducibility:** every reported aggregate can be regenerated from archived raw evidence and the versioned analysis pipeline.

## 10. Existing evidence boundary

Evidence produced before this contract remains valuable engineering evidence, regression evidence, and hypothesis-generating material. It does not automatically become definitive publication evidence merely because it is archived.

In particular, existing NY results may motivate the publication campaign, but claims requiring equal-byte controls, independent workload samples, stronger ablations, multiple graphs, or qualified external comparators must be re-earned under the corresponding later protocol.

## 11. Decision policy

At the publication viability and definitive checkpoints, the project must accept one of three evidence-driven outcomes:

### GO — Scoped SOTA

ADES establishes or materially improves the runtime-memory Pareto frontier over the qualified exact-online comparator set under the frozen scope.

### REFRAME — Strong paper, narrower claim

ADES is clearly superior only in defined operating regions, but the adaptive strategy, crossover structure, or phase diagram is scientifically meaningful.

### NO-GO FOR SOTA

Simpler strategies perform equivalently or better after fairness controls, strong baselines, equal-memory constraints, and statistical scrutiny.

The selected outcome must follow the evidence rather than the desired framing.

## 12. Change control

This contract is frozen after PR41 merges.

A later change to a central research question, candidate SOTA scope, primary metric meaning, comparator fairness rule, or central claim registry requires:

1. a new contract version;
2. a documented reason;
3. identification of which prior experiments remain comparable;
4. preservation of the superseded contract;
5. no retroactive deletion of inconvenient evidence.

Editorial clarifications that do not alter scientific meaning may be made without redefining the campaign.

## 13. PR41 acceptance checklist

PR41 is complete when all of the following are true:

- [x] RQ1 is frozen.
- [x] The candidate SOTA scope is explicit and bounded.
- [x] Primary runtime, memory, latency, comparative, and uncertainty metrics are named.
- [x] Non-claims are explicit.
- [x] Central supporting claims have stable identifiers.
- [x] Every future publication-facing experiment is required to map to explicit claim IDs.
- [x] Negative and contradictory evidence is required to remain visible.
- [x] Existing pre-contract evidence is clearly separated from definitive publication evidence.
- [x] No ADES performance logic is modified by this PR.

---

This contract governs the scientific interpretation of the publication campaign. Lower-level benchmark mechanics remain specified in `docs/BENCHMARK_PROTOCOL.md`; current implementation/evidence status remains tracked in `docs/evidence-ledger.md` until the later paper evidence ledger supersedes that role for manuscript claims.
