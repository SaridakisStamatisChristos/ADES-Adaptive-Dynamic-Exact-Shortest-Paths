# PR48 — External Comparator Eligibility and Qualification

## Purpose

PR48 defines which external shortest-path methods may be used as **direct** evidence for the ADES publication campaign and which must be segregated as literature or contextual comparisons.

**Claims addressed:** `C4`, `C7`, `C-SOTA`.

The purpose is to prevent an apparent state-of-the-art result from being created by comparing ADES only with convenient or structurally weaker baselines. It is equally important to prevent the opposite error: ranking ADES directly against methods that operate with materially different preprocessing, update, source, approximation, or memory assumptions.

PR48 qualifies comparison *contracts*. It does not require an external method to become DIRECT merely to complete the PR.

## Status classes

Every reviewed external method must have exactly one status in `benchmarks/comparators/registry.json`.

### `DIRECT`

The method may appear in a matched performance table or Pareto frontier as direct evidence for `C-SOTA`.

A DIRECT method must satisfy **every frozen hard gate** with empirical/reproducible evidence under the ADES harness. Literature statements alone are insufficient.

### `PENDING`

The method is plausibly comparable but one or more hard gates remain unproven or unimplemented.

A PENDING method may motivate engineering work and may be discussed in the paper. It must not be counted as a direct win/loss until promoted to DIRECT by a later evidence-bearing change.

### `CONTEXT`

The method is scientifically relevant, but its assumptions differ materially from the frozen C-SOTA scope. Examples include large preprocessed routing indices, fixed-goal replanning systems, approximation, or offline knowledge.

CONTEXT results may be reported in separately labeled tables or prose. They must not be folded into a direct SOTA win count or Pareto frontier without a contract revision and new qualification.

### `INELIGIBLE`

The method violates one or more hard scope requirements and is not suitable even as a candidate direct comparator under the current contract.

## Frozen DIRECT hard gates

A DIRECT external comparator must demonstrate all of the following:

1. **Exact answers** — every measured query agrees with the independent exact oracle.
2. **Online operation processing** — operations are processed in arrival order; no future trace knowledge is used.
3. **Directed graphs** — the implementation supports the directed graph model used by the campaign.
4. **Nonnegative weighted graphs** — the implementation supports the campaign weight domain.
5. **Weight increases** — arbitrary supported edge-weight increases are handled correctly.
6. **Weight decreases** — arbitrary supported edge-weight decreases are handled correctly.
7. **No future knowledge** — no offline trace optimization or future-operation planning is used.
8. **Arbitrary query sources supported** — the frozen repeated arbitrary-source stream can be executed. A fixed-source SSSP primitive may qualify only through an explicit on-demand source-state policy whose creation/promotion cost is charged.
9. **Matched trace adapter** — the comparator consumes the same canonical PR42 trace bytes and reports the same SHA-256 and operation counts.
10. **Independent oracle validation** — exactness is checked through the same publication oracle discipline.
11. **Persistent-memory budget enforceable** — comparator-owned persistent state can be kept within the PR43 declared byte budget.
12. **Persistent-memory accounting auditable** — the bytes included/excluded from the common budget are documented and machine-checkable.
13. **Preprocessing semantics declared** — all preprocessing/index construction is identified and either charged or explicitly separated by comparison class.
14. **Timing boundary declared** — query/update/preprocessing/validation timing boundaries are compatible with PR44.
15. **Implementation pinned** — executable source is pinned to an immutable revision.
16. **License compatible for reproduction** — the pinned implementation may legally be acquired/built/reproduced for the research package.

For `DIRECT`, the registry value for every gate must be exactly:

```text
PASS
```

`PASS_THEORY`, `PENDING_TEST`, `PENDING_ADAPTER`, or any contextual status is intentionally insufficient.

## Fixed-source algorithms and arbitrary-source fairness

A dynamic SSSP implementation that maintains one source is not automatically disqualified. It may qualify if the adapter defines a fair policy for the arbitrary-source stream:

- source-state construction happens only after that source appears online;
- construction cost is included in algorithm time in the corresponding operation domain;
- every retained source state is included in the persistent-state byte budget;
- eviction/destruction policy is declared;
- the adapter has no future knowledge;
- the policy does not receive source-frequency information unavailable to ADES.

Without this adapter, a fixed-source method is PENDING or CONTEXT, not DIRECT.

## Preprocessing/index fairness

Preprocessed exact routing methods can be important context but cannot be silently ranked against ADES as though preprocessing and index memory were free.

A method with topology/metric preprocessing may become DIRECT only if the publication protocol explicitly defines:

- whether preprocessing is paid once, amortized, or excluded;
- whether the index remains valid under each update;
- update/customization cost after every relevant graph change;
- persistent index bytes under the same declared memory policy;
- whether the same graph classes are supported;
- whether the comparison still answers RQ1 rather than a different routing-service question.

If those assumptions materially change RQ1, the method remains CONTEXT under contract v1.0.

## Current PR48 review

PR48 reviews five external candidates.

### NetworKit `DynDijkstra` — `PENDING`

Pinned source revision:

```text
networkit/networkit@ab420840dbb8a61fcb1ea0d89e5acc44fe66f2ab
```

The public source implements dynamic single-source Dijkstra state and exposes `update` / `updateBatch` through graph events. It is the strongest immediate adapter candidate because executable source exists and its core problem is dynamic SSSP.

It is **not DIRECT in PR48** because ADES has not yet demonstrated, through its own matched harness:

- correct arbitrary weight-increase and weight-decrease replay on the frozen directed model;
- arbitrary-source adaptation without unfair preinitialization;
- PR43-compatible persistent byte accounting/enforcement;
- PR42 trace identity and PR44 timing integration;
- independent oracle success on publication traces.

This is an actionable engineering candidate, not a rejected method.

### Ramalingam–Reps incremental SSSP — `PENDING`

Primary reference:

> G. Ramalingam and T. Reps, *An Incremental Algorithm for a Generalization of the Shortest-Path Problem*, Journal of Algorithms 21(2), 1996. DOI: `10.1006/JAGM.1996.0046`.

The paper explicitly treats positive-edge SSSP and heterogeneous graph modifications, making it central literature for ADES dynamic repair.

It remains PENDING because PR48 has not identified and pinned a maintained executable implementation suitable for matched benchmarking, memory accounting, and exact trace replay.

### Frigioni–Marchetti-Spaccamela–Nanni fully dynamic SSSP — `PENDING`

Primary references include:

- *Fully dynamic algorithms for maintaining shortest path trees*, Journal of Algorithms 34, 2000, DOI `10.1006/jagm.1999.1048`.
- *Fully dynamic shortest paths in digraphs with arbitrary arc weights*, Journal of Algorithms 49, 2003, DOI `10.1016/S0196-6774(03)00082-8`.

The theoretical problem is close to the dynamic SSSP core of ADES. It remains PENDING because no vetted executable reference implementation has been integrated and qualified under the publication harness.

### RoutingKit Customizable Contraction Hierarchies — `CONTEXT`

Pinned source revision:

```text
RoutingKit/RoutingKit@54d49bb0cdea56dde182357522e4e86a03c57852
```

RoutingKit provides exact high-performance routing with preprocessing and metric customization. It is important practical context, especially for road networks.

It is CONTEXT under contract v1.0 because topology preprocessing, index memory, and customization phases create a materially different lifecycle from the frozen ADES online edge-update stream. Those assumptions must not be silently collapsed into an ordinary per-operation direct ranking.

### D* Lite — `CONTEXT`

Primary reference:

> S. Koenig and M. Likhachev, *D* Lite*, AAAI 2002.

D* Lite is a major incremental replanning method, but its goal-directed heuristic replanning model is not the frozen arbitrary repeated-source point-to-point workload model. It therefore belongs in related-work context unless the research question is explicitly reframed.

## Current DIRECT result

At PR48 merge time:

```text
direct_external_comparators = 0
headline_sota_ready = false
```

This is an intentional and scientifically meaningful result.

The repository has now demonstrated that relevant external methods exist, but none has yet satisfied every empirical DIRECT gate. Therefore:

- `C-SOTA` remains **UNESTABLISHED**;
- no PENDING or CONTEXT comparator may be represented as a direct ADES win;
- the next campaign stages may still test internal controls and publication viability;
- a final headline SOTA claim requires either promotion of an external candidate to DIRECT or an explicit, defensible manuscript scope that does not overstate the comparator set.

## Promotion from PENDING to DIRECT

A later PR may promote a comparator only by adding all of the following:

1. pinned build/reproduction instructions;
2. a canonical trace adapter;
3. exactness tests against the independent oracle;
4. directed mixed increase/decrease coverage;
5. arbitrary-source policy documentation and tests;
6. PR43-compatible persistent memory accounting and hard enforcement;
7. PR44-compatible timing semantics;
8. matched multi-graph evidence;
9. a registry change in which **every** hard gate is `PASS`;
10. CI that fails if any of those properties regress.

A registry status edit without the evidence above is invalid.

## No favorable-comparator filtering

External methods may not be excluded merely because they outperform ADES.

Exclusion is valid only for a predeclared eligibility reason grounded in the frozen contract. Once an external method is DIRECT-qualified for a campaign stage, its losses and wins remain in the evidence package under the same no-favorable-cell-filtering rule as ADES results.

## PR48 acceptance criterion

PR48 is complete when:

- the DIRECT eligibility rules are explicit;
- reviewed candidates have machine-readable statuses and evidence/blockers;
- assumption-mismatched methods are segregated rather than misranked;
- the registry cannot label a method DIRECT with unresolved gates;
- zero DIRECT external comparators is permitted but automatically blocks headline-SOTA readiness;
- CI emits a qualification summary that later stages can consume.
