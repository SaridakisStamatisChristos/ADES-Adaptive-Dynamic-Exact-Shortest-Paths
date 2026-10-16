# PR52 — Progressive Frontier Residency (ADES-V4)

## Purpose

PR50 and PR51 established that the remaining ADES performance problem is an online rent-vs-buy conflict between repeated cold point-to-point search and eager full-source materialization. V2 can wait too long; V3 can buy too early and thrash. PR52 changes the state space rather than tuning that threshold again.

`ADES-V4` introduces a third state:

```text
COLD -> PROGRESSIVE FORWARD FRONTIER -> FULL REPAIRABLE SSSP
```

Historical `ADES`, `ADES-V2`, and `ADES-V3` code paths are unchanged.

## Exact progressive query

A progressive source retains the forward half of Dijkstra across targets while the graph epoch is unchanged. Each query combines that persistent forward state with a fresh backward Dijkstra search. The bidirectional stopping rule therefore still returns an exact point-to-point distance; no approximation or cached answer substitution is permitted.

The forward representation is deliberately dense and bounded:

- `dist[V]`;
- selected `parent_edge[V]`;
- indexed-heap position `heap_pos[V]`;
- heap vertex slots with a semantic capacity of `V`.

The indexed heap prevents duplicate-key growth. PR52 charges all `V` heap slots up front, even when the live heap is smaller. This makes the logical persistent-state maximum known when the partial source is created.

## Progressive completion

When measured incremental value justifies full residency, V4 finishes only the unsettled forward work. Existing distance and parent arrays are moved into the ordinary `SSSPState`; tree-link and repair metadata are then materialized. It does not restart Dijkstra from the source.

The promotion decision is intentionally delayed until at least three source observations. Its benefit is measured against the observed progressive-query cost, not against a hypothetical cold query, so a partial source that is already cheap to query does not automatically justify a larger full resident.

## Dynamic updates

Speculative partial frontiers are not repaired in PR52. Any real graph-weight change increments a global graph epoch in O(1). A partial source whose epoch is stale is discarded lazily when queried or when partial memory must be reclaimed.

Full residents continue to use the existing exact local-repair / rebuild controller.

This split is intentional:

- speculative state remains cheap to maintain under updates;
- only sources whose measured value pays for full residency incur dynamic maintenance.

## Memory fairness

PR43 logical persistent-state accounting remains the hard fairness mechanism.

ADES-V4 accounts:

- the fixed 64-slot recent-source table;
- every progressive source at its full vertex-bounded semantic capacity;
- V4 partial-source economic metadata;
- every full `SSSPState`;
- per-resident economic and repair-controller metadata.

Temporary backward-search arrays and conversion workspaces remain temporary algorithm work just as temporary search structures are excluded from the existing persistent-state metric. Peak RSS is measured independently.

A full promotion may replace its own partial state and, if necessary, a lower-value full resident. It is not allowed to make space by silently deleting unrelated progressive states; this prevents promotion from reintroducing V3-style speculative churn.

## Evidence separation

### Regression phase

The regression phase intentionally reuses known PR50/PR51 hard cells:

- NY and grid churn cells where delayed materialization lost to B3L;
- NY and scale-free hot-pool cells where V3 over-promoted relative to V2.

These cells are tuning/regression evidence only.

### Fresh third holdout

Frozen before any ADES-V4 full performance result.

Profiles:

```text
COLD
B3L
FREQ-LRU-REPAIR
ADES
ADES-V2
ADES-V3
ADES-V4
```

Graphs:

```text
syn-grid-224x224-v1
syn-uniform-50000-d6-v1
syn-clustered-50000-c100-d6-v1
```

Source families/locality:

```text
zipf @ 75%
churn @ 90%
```

Update scenarios:

```text
decrease-only / medium
strict-alternating / small
```

Update intervals:

```text
1/5
1/50
```

Persistent-state budgets:

```text
20 MiB
40 MiB
```

Independent seeds:

```text
211
257
307
```

Full trace length: 160 queries.

The cross-product contains 48 holdout3 regimes. The graph/family/update/budget/seed combinations are distinct from the frozen PR50 and PR51 holdouts.

## Claims addressed

```text
claim_ids: [C1, C2, C3, C4, C8, C9]
```

PR52 does not by itself establish `C-SOTA`; external DIRECT comparator qualification remains a separate requirement.

## Hard validity gates

Any of the following invalidates a measured run:

- shortest-path answer disagrees with the independent oracle;
- trace SHA-256 differs across compared profiles;
- configured byte budget differs;
- peak accounted persistent bytes exceed the budget;
- incomplete/malformed evidence output.

Performance losses are preserved and never converted into CI correctness failures.

## Predeclared performance target

Primary fresh-holdout target:

> **Zero statistically material >=10% ADES-V4 regressions against COLD, B3L, FREQ-LRU-REPAIR, ADES-v1, ADES-V2, or ADES-V3 under the paired 95% confidence calculation.**

Secondary target:

> ADES-V4 is fastest or within 1% of the fastest internal profile in as many holdout3 regimes as possible.

These are empirical targets, not a theorem of universal online dominance. The frozen publication contract's no-future-knowledge boundary remains unchanged.

## Scientific change control

After the first full holdout3 execution, holdout3 is consumed. Any V4 tuning informed by its outcome requires a new holdout for independent confirmation. Negative and contradictory evidence must remain archived.
