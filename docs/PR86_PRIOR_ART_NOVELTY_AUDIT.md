# PR86 — Focused prior-art and novelty audit

## Decision

**NO-GO for broad algorithmic/mechanism novelty.**

**POSSIBLY DISTINCT systems integration, but novelty remains UNESTABLISHED.**

This audit does not alter the independent PR81–PR84 conclusion that ADES has **scoped practical SOTA evidence** under the frozen Publication Contract v1.0. Performance-SOTA and scholarly novelty are separate claims.

PR86 is deliberately collision-oriented. It asks whether the actual frozen ADES mechanisms, as reconciled in PR85 against candidate `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`, can support scholarly novelty claims. A literature search that fails to find a close match is not treated as proof of novelty.

## Claims addressed

```text
claim_ids: [C4, C5, C6, C7, C9, C-SOTA]
```

PR86 does **not** add a new novelty claim ID to Publication Contract v1.0. Novelty remains an external manuscript/paper contribution question rather than a retroactive rewrite of the frozen performance contract.

## Audit labels

- `PRIOR_ART_COLLISION` — the mechanism/general idea is already clearly present in pre-ADES literature.
- `PARTIAL_PRIOR_ART_COLLISION` — important components/decision logic are established, but the exact ADES specialization differs.
- `CONTEMPORARY_COLLISION` — closely related 2026 work exists, but priority is not resolved by this audit and it is not used alone to establish pre-ADES prior art.
- `NO_CLOSE_MATCH_FOUND` — the focused search did not identify an exact close match; this is **not** a novelty finding.
- `NOVELTY_UNESTABLISHED` — evidence is insufficient for a defensible novelty claim.

## N1 — exact dynamic SSSP repair under weight changes

**Classification: `PRIOR_ART_COLLISION`.**

ADES's general idea of preserving a shortest-path state and repairing only affected regions after online edge-weight changes is not novel.

Key prior art:

1. G. Ramalingam and T. Reps, **“An Incremental Algorithm for a Generalization of the Shortest-Path Problem,”** *Journal of Algorithms*, 1996, DOI `10.1006/JAGM.1996.0046`.
   - Supports multiple heterogeneous modifications including edge insertions, deletions and edge-length changes.
   - https://research.ibm.com/publications/an-incremental-algorithm-for-a-generalization-of-the-shortest-path-problem
2. D. Frigioni, A. Marchetti-Spaccamela and U. Nanni, **“Fully Dynamic Algorithms for Maintaining Shortest Paths Trees,”** *Journal of Algorithms*, 2000, DOI `10.1006/jagm.1999.1048`.
   - Maintains distances and SPTs under insertions, deletions and weight updates.
   - https://www.sciencedirect.com/science/article/abs/pii/S0196677499910489
3. D. Frigioni et al., **“Fully Dynamic Shortest Paths in Digraphs with Arbitrary Arc Weights,”** 2001 technical report / later journal publication.
   - Directed fully dynamic SSSP, including weight increases/decreases and zero-length cycles.
   - https://www.cs.au.dk/~gerth/alcom-ft/TR/ALCOMFT-TR-01-31.html
4. A. Narváez, K.-Y. Siu and H.-Y. Tzeng, **dynamic shortest-path-tree update** work (2000–2001), another independent dynamic-SPT lineage.

**Permitted manuscript framing:** ADES uses established dynamic-SSSP principles in a new engineering architecture; do not claim invention of dynamic repair itself.

## N2 — filter an increased edge that is not the selected predecessor/tree edge

**Classification: `PRIOR_ART_COLLISION`.**

The PR54 rule

> if an increased edge is not the selected predecessor/parent edge, no SPT recomputation is necessary

has explicit pre-ADES precedent.

A. Tripathi, R. R. Sharma, M. Khandelwal and T. Mehta, **“Incremental parallel computing for continuous queries in dynamic graphs using a transactional model,”** *Concurrency and Computation: Practice and Experience* 30(18), 2018, DOI `10.1002/cpe.4500`, states that if an updated edge is a non-predecessor edge and its length increases, no incremental computation is required.

- https://experts.umn.edu/en/publications/incremental-parallel-computing-for-continuous-queries-in-dynamic-/

The same predecessor-vs-non-predecessor distinction appears in related transactional dynamic-graph work and is consistent with classic shortest-path-tree update reasoning.

**Conclusion:** ADES's specific proof is useful and the O(1) implementation is valuable, but the underlying rule is not a defensible novelty claim.

## N3 — alternate shortest-path witness rescue after an affected tree edge increases

**Classification: `PRIOR_ART_COLLISION` for the concept; ADES gating is an implementation specialization.**

Replacement/alternative witness handling is old dynamic shortest-path technique.

1. V. King and M. Thorup, **“A Space Saving Trick for Directed Dynamic Transitive Closure and Shortest Path Algorithms,”** COCOON 2001, DOI `10.1007/3-540-44679-6_30`.
   - The decremental shortest-path structure scans incoming edges for a replacement witness when a selected witness disappears.
   - https://dblp.org/rec/conf/cocoon/KingT01
2. The Ramalingam–Reps family and later reduced-heap/dynamic-SPT algorithms explicitly exploit alternative shortest paths/witnesses to avoid unnecessary propagation.
3. Later dynamic-SPT literature describes identifying alternative routes after tree-edge increases.

PR57-v2's exact policy — no scan below 256 affected vertices, one scan of at most 64 incoming arcs at the trigger, immediate acceptance of strictly-smaller-distance tight predecessors and deferred equal-distance acceptance after subtree-membership proof — is a **bounded engineering specialization**. Numeric trigger constants chosen for performance are not, by themselves, a meaningful scholarly novelty claim.

**Permitted framing:** bounded witness probing is an ADES optimization/design choice whose empirical value can be evaluated; do not claim invention of alternate-witness dynamic repair.

## N4 — bounded residency of exact per-source SSSP state for repeated arbitrary sources

**Classification: `PARTIAL_PRIOR_ART_COLLISION` + `NO_CLOSE_MATCH_FOUND`; novelty remains `NOVELTY_UNESTABLISHED`.**

The broad ingredients are well established:

1. C. S. Jensen, J. R. Thomsen and M. L. Yiu, **“Effective caching of shortest paths for location-based services,”** SIGMOD 2012, DOI `10.1145/2213836.2213872`.
   - Uses query-log statistics to estimate caching benefit and places beneficial shortest paths in a bounded cache.
   - https://pure.au.dk/portal/en/publications/effective-caching-of-shortest-paths-for-location-based-services/
2. X. Li et al., **“Refreshment of the shortest path cache with change of single edge,”** *Expert Systems with Applications* 67 (2017), DOI `10.1016/j.eswa.2016.08.011`.
   - Explicitly studies shortest-path caches on changing graphs, affected-cache detection and refresh strategies.
   - https://www.sciencedirect.com/science/article/abs/pii/S0957417416304043
3. K. Ammar, S. Sahu, S. Salihoglu and M. T. Özsu, **“Optimizing Differentially-Maintained Recursive Queries on Dynamic Graphs,”** PVLDB 15(11), 2022, DOI `10.14778/3551793.3551862`.
   - Demonstrates the performance/memory tradeoff of retaining incremental state for multiple continuous shortest-path/recursive queries and selectively dropping/recomputing state to meet memory pressure.
   - https://vldb.org/pvldb/vol15/p3186-ammar.pdf
4. Multiple-standing-query dynamic-graph systems also explicitly trade maintained query count against update work and memory.

The focused search did **not** identify a pre-ADES paper matching all of these ADES specifics simultaneously:

- arbitrary online `(source,target)` queries;
- source-level promotion into full exact SSSP state;
- a hard auditable persistent-byte budget;
- causal eviction among resident source states;
- the same resident states being kept exact under mixed online weight increases/decreases;
- cold exact bidirectional search as the fallback for nonresidents.

That gap is interesting, but `NO_CLOSE_MATCH_FOUND` is not proof of novelty. A paper may reasonably present this as an integrated systems design and compare it with prior shortest-path caching, standing-query maintenance and memory-bounded incremental computation; it should not state “first bounded multi-source exact SSSP cache” without a broader systematic review.

### Contemporary collision

A 2026 work, Ankush Rai's **ALSSSP: A Zero-Preprocessing Framework for Adaptive Single-Source Shortest Path Computation with Provable Guarantees** (Zenodo DOI `10.5281/zenodo.19184398`, March 2026), describes a multi-level cache including complete SSSP trees for hot sources and LRU-bounded caching. This is a close contemporary collision for hot-source tree residency, although its dynamic-update semantics differ from ADES and PR86 does not resolve publication priority between the projects.

**Classification for priority purposes: `CONTEMPORARY_COLLISION`, not evidence of pre-ADES priority.**

## N5 — cold-work-aware admission

**Classification: `PARTIAL_PRIOR_ART_COLLISION`; standalone novelty rejected.**

ADES admits a source based on observed cold-query work rather than recurrence alone. The graph-specific metric (cold scanned-edge work relative to graph size) is specialized, but the underlying principle — admit cached state when estimated future saved miss cost justifies cache occupancy — is established.

Relevant prior art:

1. G. Einziger, R. Friedman and B. Manes, **“TinyLFU: A Highly Efficient Cache Admission Policy,”** *ACM Transactions on Storage* 13(4), 2017, DOI `10.1145/3149371`.
   - Candidate-vs-victim cache admission using observed recent access frequency.
2. **“Boosting Cache Performance by Access Time Measurements,”** *ACM Transactions on Storage*, DOI `10.1145/3572778`.
   - Cost Aware TinyLFU incorporates measured miss/hit access-time benefit into admission decisions, rather than optimizing hit ratio alone.
3. Query-result caching for continuous queries has long used estimated cache-production/reuse cost and expected gains.

**Potentially distinctive part:** using exact shortest-path search work (e.g. scanned edges) as the measured miss-cost signal for deciding whether to materialize a full source state. Treat this as an application-specific design, not an established novelty claim until a larger search shows the exact policy is absent.

## N6 — hit/age/update-debt-aware eviction under a byte budget

**Classification: `PRIOR_ART_COLLISION` at the general policy level; exact ADES score is an implementation heuristic.**

ADES combines observed reuse, recency and maintenance burden in its resident-source victim score. This is closely analogous to mature cache/materialization-selection work that balances:

- access frequency;
- recency;
- retrieval/recomputation cost;
- maintenance/update cost;
- storage/resource constraints.

Relevant precedents include:

1. TinyLFU / LRU / adaptive cache-policy literature.
2. Cost-aware caching using access-time benefit, DOI `10.1145/3572778`.
3. H. Gupta and I. S. Mumick, **“Selection of views to materialize under a maintenance cost constraint,”** 1998/ICDT 1999.
   - Explicitly selects retained materialized state to reduce query cost subject to maintenance/resource constraints.
   - https://researchconnect.suny.edu/en/publications/selection-of-views-to-materialize-under-a-maintenance-cost-constr/

The exact heuristic

`hits / (1 + age + lambda * updateDebt)`

is ADES-specific, but a new algebraic combination of established cache/maintenance signals is not, by itself, enough for a strong algorithmic novelty claim.

## N7 — adaptive local-repair versus full-recompute controller

**Classification: `PRIOR_ART_COLLISION` at the general decision principle.**

K. Bok et al., **“Cost Model Based Incremental Processing in Dynamic Graphs,”** *Electronics* 11(4):660, 2022, DOI `10.3390/electronics11040660`, explicitly uses measured/history-based cost statistics to choose between incremental processing of an affected graph region and full static processing when recomputation is predicted to be cheaper.

- https://www.mdpi.com/2079-9292/11/4/660

This directly collides with any broad claim that ADES invented the idea of estimating local incremental cost versus full recomputation cost on a dynamic graph.

ADES's controller remains interesting as an SSSP-specific implementation with exact fallback and detailed work telemetry. Its particular estimator, hard safety caps, and empirical refutation of “large affected subtree ⇒ rebuild” can be manuscript contributions/engineering findings, but not a broad algorithmic novelty claim.

## N8 — integrated ADES architecture

**Classification: `NO_CLOSE_MATCH_FOUND` + `NOVELTY_UNESTABLISHED`.**

The focused audit did not identify one pre-ADES system that exactly combines all of:

1. exact online directed weighted shortest paths;
2. arbitrary repeated query sources;
3. cold exact point-to-point bidirectional search;
4. source-level materialization of full exact SSSP/SPT state;
5. a hard auditable persistent-state byte cap;
6. causal source admission/eviction using observed query and maintenance economics;
7. exact incremental weight-decrease/increase repair for every resident source;
8. conservative rebuild fallback;
9. bounded alternate-witness rescue;
10. an adaptive measured repair/rebuild controller;
11. no future operation knowledge.

However, nearly every ingredient has substantial prior art, and the focused search is not an exhaustive systematic review of all databases, routing, incremental-computation, dynamic-graph and caching literature. Therefore the only defensible status is:

`POSSIBLY_DISTINCT_SYSTEMS_INTEGRATION — NOVELTY_UNESTABLISHED`

A systems paper can still make a meaningful contribution without claiming that each mechanism is new. The strongest current framing is the **integration, exactness/fairness methodology, adaptive operating regime, and measured runtime-memory frontier**, not invention of dynamic shortest-path repair or generic adaptive caching.

## Prior-art map

| ADES element | PR86 status | Strongest collision |
|---|---|---|
| exact dynamic SSSP repair | `PRIOR_ART_COLLISION` | Ramalingam–Reps 1996; Frigioni et al. 2000/2001 |
| non-parent increase O(1) filter | `PRIOR_ART_COLLISION` | Tripathi et al. 2018 predecessor-edge rule |
| alternate witness rescue | `PRIOR_ART_COLLISION` (concept) | King–Thorup 2001 replacement witness; RR-family alternatives |
| bounded exact source residency | `PARTIAL_PRIOR_ART_COLLISION`; `NO_CLOSE_MATCH_FOUND` | shortest-path caching; dynamic cache refresh; multi-query incremental state |
| work-aware admission | `PARTIAL_PRIOR_ART_COLLISION` | TinyLFU + cost-aware cache admission |
| reuse/recency/update-debt eviction | `PRIOR_ART_COLLISION` (general principle) | cache replacement + materialized-view maintenance selection |
| repair-vs-rebuild cost controller | `PRIOR_ART_COLLISION` (general principle) | iGAS cost model 2022 |
| exact integrated ADES combination | `NO_CLOSE_MATCH_FOUND`; `NOVELTY_UNESTABLISHED` | no exact close match found in focused audit |

## Manuscript consequences

### Allowed

- “ADES integrates exact dynamic SSSP maintenance with bounded adaptive source residency and cost-aware online policy under a reproducible exact-online benchmark contract.”
- “We are not aware, from the focused prior-art audit reported here, of one preexisting system with the exact full ADES combination.”
- “The contribution is primarily systems integration, exactness/fairness discipline, operating-regime analysis and measured runtime-memory performance.”
- mechanism-specific empirical contributions, e.g. showing when a bounded witness probe helps or when affected-region size is a poor rebuild proxy.

### Not currently allowed

- “ADES invents dynamic SSSP repair.”
- “ADES is the first to ignore non-tree/non-predecessor edge increases.”
- “ADES invents alternate shortest-path witness repair.”
- “ADES is the first cost-aware adaptive cache.”
- “ADES invents choosing incremental repair versus recomputation using measured costs.”
- “ADES is the first bounded hot-source shortest-path cache.”
- “The ADES architecture is novel” without a broader systematic prior-art review and a precise novelty claim narrower than the current integration description.

## Relationship to scoped practical SOTA

PR86 **does not reverse or weaken** PR81's scoped practical-SOTA decision.

Performance state-of-the-art is an empirical comparative claim under a declared scope. Scholarly novelty is a priority/originality claim about ideas. ADES can have strong scoped practical-SOTA evidence even when many internal mechanisms have prior art.

The current combined verdict is therefore:

- **performance:** `GO — SCOPED PRACTICAL SOTA` under Publication Contract v1.0;
- **broad algorithmic novelty:** `NO-GO`;
- **narrow integrated systems novelty:** `UNESTABLISHED / POSSIBLY DISTINCT`, requiring a broader systematic review before promotion.

## Next R7 action

Do not rewrite mechanisms merely to manufacture novelty.

The highest-value next step is publication construction around the contribution that actually survives:

1. exact adaptive bounded-residency systems architecture;
2. strong scoped performance evidence versus two DIRECT exact-online comparators;
3. road/non-road source-reuse generalization;
4. explicit mechanism ablations and negative findings;
5. transparent prior-art positioning;
6. artifact-level reproducibility.

If stronger novelty is desired, formulate one **narrow, falsifiable integration claim** and run a dedicated systematic search for that exact claim. Until then, keep `G1` from PR83 open.