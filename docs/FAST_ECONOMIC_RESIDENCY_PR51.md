# PR51 — Fast Economic Residency (ADES-v3)

## Purpose

PR50 established that predictive economic residency generalizes, but also preserved two failure mechanisms:

1. profitable churn regimes can spend too many early queries cold before resident state is materialized;
2. a workload that remains completely cold can lose to ADES-v1 purely from predictor/controller overhead.

PR51 attacks only those mechanisms. It does not alter exact shortest-path semantics or the local repair algorithms.

## Historical profiles remain frozen

- `ADES` / `FullADES`: historical v1 used by PR45–PR49.
- `ADES-V2` / `PredictiveEconomic`: frozen PR50 controller.
- `ADES-V3` / `PredictiveEconomicFast`: PR51 controller.

PR49 and PR50 evidence remain development/history evidence and are not relabeled as fresh confirmation.

## V3 nonresident predictor

V2 promotes a repeated source into an `economic_sources_` map, maintains reuse/cost EWMA there, and recomputes logical accounting after metadata changes.

V3 keeps nonresident prediction entirely inside the already-accounted 64-slot direct-mapped recent-source table.

For a nonresident source:

1. first sight performs the exact cold bidirectional query and writes one fixed slot;
2. if the same source is observed again while its slot survives, the observed reuse gap plus stored cold-query cost/work form a temporary economic candidate;
3. if expected value is positive and capacity-aware replacement accepts the candidate, the triggering query runs one full Dijkstra and retains that same SSSP state;
4. otherwise the query remains cold and refreshes the fixed slot.

No per-source heap metadata is allocated while the source remains nonresident. Updating the fixed table cannot change logical persistent-state bytes, so V3 does not run a full memory-accounting traversal after every cold query.

## Early promotion

V3 uses a 32-query finite economic horizon versus V2's frozen default of 16. This is not a universal-optimality assumption; it is a development hypothesis motivated by the PR50 churn failures.

The economic decision still charges:

- estimated full-SSSP build cost;
- measured/estimated resident maintenance cost;
- byte-budget capacity;
- replacement opportunity cost through the same economic resident-value comparison.

A short reuse gap therefore does not automatically imply promotion. Expensive full-state graphs can remain cold when projected build payback is negative.

## Exactness and fairness invariants

Unchanged hard requirements:

- exact query answer must match the independent oracle bundle;
- canonical trace hash must match for every profile;
- graph/update semantics are identical across profiles;
- `peak_accounted_algorithm_state_bytes <= budget_bytes` for every run;
- performance losses never invalidate or disappear from the evidence.

The V3 recent-source table and resident economic metadata are included in the PR43 logical state budget.

## Evaluation separation

### Regression phase

The regression phase deliberately reuses consumed evidence:

- NY/grid churn cells associated with PR50 delayed promotion;
- the clustered/hot-pool/repeated-edge PR50 holdout contradiction.

These results test whether the intended mechanisms moved in the expected direction. They are not fresh scientific confirmation.

### Second holdout (`holdout2`)

Frozen before observing V3 full-run performance.

Profiles:

- `COLD`
- `B3L`
- `FREQ-LRU-REPAIR`
- `ADES`
- `ADES-V2`
- `ADES-V3`

Independent seeds:

- 101
- 137
- 181

Byte budgets:

- 24 MiB
- 48 MiB

Graph classes / pairings:

- NY road distance
- scale-free synthetic
- small-world synthetic

Source locality:

- hot-pool at 70%
- rotating-hot at 80%

Update regimes:

- balanced-random, small perturbations
- increase-only, large perturbations

Update intervals:

- 1/10
- 1/100

These combinations, seeds, byte budgets, and update-direction/magnitude settings were not the PR50 holdout matrix.

## Predeclared interpretation

Primary second-holdout target:

> zero statistically material >=10% regressions of ADES-V3 against any frozen internal comparator under the paired 95% confidence calculation.

Secondary evidence:

- number of regimes where V3 is outright fastest;
- number where V3 is fastest or within 1%;
- material pairwise wins;
- cold-query/promotion counts for mechanistic interpretation.

This target is empirical and scoped. It is not a theorem that an online algorithm without future knowledge can dominate every operation sequence.
