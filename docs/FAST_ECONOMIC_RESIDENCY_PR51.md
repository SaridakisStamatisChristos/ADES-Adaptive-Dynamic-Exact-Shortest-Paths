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

V3 uses a 32-query finite economic horizon versus V2's frozen default of 16. This was a development hypothesis motivated by the PR50 churn failures.

The economic decision still charges estimated full-SSSP build cost, measured/estimated resident maintenance cost, byte-budget capacity, and replacement opportunity cost through the economic resident-value comparison.

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

The regression phase deliberately reuses previously observed failure cells. These results are tuning/regression evidence only.

### Second holdout (`holdout2`)

Frozen before observing V3 full-run performance.

Profiles: `COLD`, `B3L`, `FREQ-LRU-REPAIR`, `ADES`, `ADES-V2`, `ADES-V3`.

Independent seeds: 101, 137, 181.

Byte budgets: 24 MiB and 48 MiB.

Graph classes / pairings: NY road distance, scale-free synthetic, small-world synthetic.

Source locality: hot-pool at 70% and rotating-hot at 80%.

Update regimes: balanced-random/small perturbations and increase-only/large perturbations.

Update intervals: 1/10 and 1/100.

Full-evaluation trace length: 120 queries per trace.

These combinations, seeds, byte budgets, and update-direction/magnitude settings were not the PR50 holdout matrix.

### Pre-evidence protocol correction

The first full-run attempt used 60 queries. The harness rejected the matrix before producing a performance result because `1/100` yields zero updates in a 60-query trace, causing nominally different update scenarios to collapse to identical trace SHA-256 values.

No PR51 performance summary or holdout decision was produced by that failed attempt. The controller remained frozen and unchanged. The protocol was corrected to 120 queries per trace, guaranteeing one update at `1/100`; the duplicate-trace guard remained enabled.

## Predeclared interpretation

Primary second-holdout target:

> zero statistically material >=10% regressions of ADES-V3 against any frozen internal comparator under the paired 95% confidence calculation.

This target is empirical and scoped; it is not a theorem that an online algorithm without future knowledge can dominate every operation sequence.

## Frozen evaluation

Frozen controller source:

`95163a3f260548439756abd20075813933715705`

Corrected 120-query evaluation commit:

`8363cd090686c00eb7ffad137f2579c3e44961fe`

GitHub Actions full run:

`37897020318`

The full run completed successfully. No shortest-path exactness, trace/oracle identity, or persistent-state-budget failure invalidated the execution.

### Machine summary

- regression regimes: 9
- holdout2 regimes: 48
- regression V3 fastest or within 1%: 1 / 9
- holdout2 V3 fastest or within 1%: 26 / 48
- regression material regressions: 6
- holdout2 material regressions: 6
- regression material pairwise wins: 25
- holdout2 material pairwise wins: 105

Fastest-profile counts on holdout2:

- ADES-V3: 20
- ADES-V2: 15
- COLD: 10
- FREQ-LRU-REPAIR: 2
- ADES-v1: 1

ADES-V3 therefore becomes the most frequent outright winner on the fresh second holdout, but the predeclared zero-material-regression target is **not met**.

## Mechanistic interpretation of the contradiction

The six statistically material holdout2 regressions are all against ADES-V2 and cluster in hot-pool workloads.

The dominant failure is over-aggressive materialization:

- in NY 24 MiB hot-pool cells, ADES-V2 typically performs about 2–3 promotions, while ADES-V3 performs roughly 14–17 promotions with 12–15 evictions;
- in several scale-free hot-pool cells, ADES-V2 remains entirely cold while ADES-V3 materializes about four resident sources and becomes slower.

Thus PR51 removes nonresident per-source predictor allocation/accounting overhead, but extending the prediction horizon to accelerate promotion overshoots in memory-constrained or high-build-cost regimes. The remaining issue is not shortest-path exactness or repair correctness; it is the economic switching policy's replacement/build-risk calibration.

This negative evidence is preserved. Holdout2 is now consumed and must not be used as fresh confirmation after further tuning.

## Permanent evidence

The completed run is permanently preserved under:

`evidence/pr51-fast/8363cd090686c00eb7ffad137f2579c3e44961fe/`

It includes browsable measurements, summaries, confidence table, analysis, decision, manifest, checksums, and `compact-evidence.tgz` containing the canonical regression/holdout2 traces and exact oracle bundles.

## PR51 conclusion

**PR51 is an informative but non-dominating iteration.**

ADES-V3 improves the controller architecture and is the most frequent winner on the fresh second holdout, but its early-promotion policy introduces six material regressions relative to frozen ADES-V2. Therefore V3 should not replace V2 as the default research candidate solely on PR51 evidence.

The next iteration should preserve both V2 and V3, add explicit promotion-risk / replacement-churn controls, and evaluate only on a new third holdout.