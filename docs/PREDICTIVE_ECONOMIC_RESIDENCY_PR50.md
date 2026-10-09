# PR50 — Predictive Economic Residency Controller (ADES-v2)

## Status and scope

PR50 is a post-PR49 engineering/research iteration. It does **not** rewrite the historical `FullADES` / `B4` algorithm used in PR45–PR49. The new controller is a distinct profile:

```text
ADES-V2
ScientificAblation::PredictiveEconomic
```

The purpose is to test whether ADES can behave more like the locally appropriate strategy across the workload phase diagram while preserving exactness, online execution, and equal-byte fairness.

This PR does not claim that an online algorithm with no future knowledge can be strictly fastest on every possible operation sequence.

## Root cause addressed

PR49 showed genuine strategy crossovers. Static COLD and resident B3L strategies each won many cells, while ADES-v1 won only a subset. Code audit identified four controller-level causes:

1. ADES-v1 admission uses accumulated historical work rather than predicted future payback.
2. Candidate and victim admission scores are expressed in incompatible units.
3. The promotion-triggering query runs bidirectional Dijkstra and then runs a second full Dijkstra to build resident state.
4. Update debt counts updates rather than measured maintenance cost.

PR50 addresses those controller costs without changing the exact repair algorithms.

## Economic decision model

For a repeated nonresident source, ADES-v2 estimates a finite-horizon value in nanoseconds:

```text
candidate_value =
    predicted_future_reuses * estimated_cold_query_ns
  - estimated_full_sssp_build_ns
  - expected_resident_maintenance_ns
```

A candidate is considered only after at least `economic_min_observations` observations. Reuse is estimated from an EWMA of observed reuse gaps over a finite `economic_horizon_queries` horizon.

Before a direct build timing exists, full-SSSP build cost is estimated from the ratio of graph edge count to observed cold-query edge scans. The estimate is replaced by an EWMA of directly measured full-build time after fused promotions occur.

Expected maintenance cost uses measured resident-update nanoseconds rather than raw update count.

## Capacity-aware replacement

A resident source has a comparable future value:

```text
resident_value =
    predicted_future_reuses * estimated_cold_query_ns
  - expected_resident_maintenance_ns
```

Build cost is not subtracted from an already-built resident because it is sunk cost.

When budget/capacity requires eviction, the candidate must exceed the lowest resident value by `economic_replacement_margin`. This is intended to prevent one-slot thrashing when several similarly valuable sources compete for capacity.

## Fused promotion

ADES-v1 discovers promotion only after completing the cold bidirectional query, then builds a full SSSP state separately.

ADES-v2 makes the economic decision **before** choosing the query algorithm once sufficient historical evidence exists:

- uneconomic candidate → execute bidirectional Dijkstra;
- economic candidate → execute one full Dijkstra, answer the target from that state, and retain the same state.

The query therefore pays one shortest-path construction rather than cold search plus full resident construction.

## First-sight fast path

ADES-v2 uses a fixed-size direct-mapped recent-source table for first observations. Rich per-source economic metadata is created only after a source repeats.

This prevents a stream of unique sources from growing unbounded probation state. The fixed table itself is persistent algorithm state and is charged to the byte budget.

## Persistent-state accounting

The PR43 logical accounting model remains the fairness mechanism. PR50 adds explicit logical accounting helpers for:

- repeated-source economic history;
- per-resident economic history;
- fixed recent-source slots.

All new persistent predictor state contributes to `accounted_algorithm_state_bytes` and `peak_accounted_algorithm_state_bytes`.

## Historical compatibility

`ScientificAblation::FullADES` remains:

```text
WorkAware admission
DebtAware eviction
LocalRepair maintenance
hysteresis enabled
cooldown enabled
```

PR50 adds a new enum value rather than modifying those settings or their execution path.

The frozen PR49 orchestration profile list is unchanged. `ades_pilot_profile` merely gains the ability to execute `ADES-V2` when explicitly requested.

## Evidence separation

### Development matrix

PR50 reruns the exact 36 PR49 runtime-memory regimes with ADES-v2 added. Because those regimes motivated the new controller, these results are **development/tuning evidence only**.

Profiles:

```text
COLD
B3L
FREQ-LRU-REPAIR
ADES
ADES-V2
```

### Fresh holdout matrix

Independent holdout evidence uses inputs not present in PR49:

Graphs:

```text
uniform-random
small-world
clustered
```

Locality families:

```text
hot-pool
zipf
rotating-hot
```

Update intervals:

```text
1/2
1/10
```

Update modes:

```text
bursty
repeated-edge
```

Seeds:

```text
43
59
83
```

Budgets remain 16 MiB and 64 MiB to preserve the memory-pressure dimension.

## Performance interpretation

Performance outcomes are evidence, not CI correctness conditions.

Hard failures:

- any wrong shortest-path answer;
- trace identity mismatch;
- oracle mismatch;
- persistent-state budget violation;
- malformed/incomplete evidence output.

A performance loss does **not** fail the workflow. Losses remain in the raw and confidence tables.

Development target:

> ADES-v2 is fastest or within 1% of the fastest measured control in as many of the 36 PR49 regimes as possible.

Holdout target:

> zero statistically material regressions of at least 10% against COLD, B3L, FREQ-LRU-REPAIR, or ADES-v1 under the predeclared paired confidence calculation.

The holdout target is evidence-calibrated; it is not a universal online-optimality theorem.

## Frozen evaluation point

The ADES-v2 controller was frozen before the first full development+holdout execution at source commit:

```text
3fb5f57a19c477742c0a87bac6f901f53b37d732
```

The complete evaluation executed in GitHub Actions run:

```text
37892264840
```

No controller changes were made between freezing the source and observing the development or holdout result.

## Frozen evaluation result

The full run completed successfully with no shortest-path exactness failure, trace/oracle identity failure, or persistent-state budget violation.

Machine summary:

```text
development regimes                         36
holdout regimes                             72
development V2 fastest or within 1%         22 / 36
holdout V2 fastest or within 1%             34 / 72
development material regressions             6
holdout material regressions                 1
development material comparator wins        33
holdout material comparator wins           145
```

`material comparator wins` counts statistically material ADES-v2 wins across comparator/regime comparison rows, not unique regimes.

### Fastest-profile counts

Development, 36 regimes:

```text
ADES-V2              19
B3L                    6
ADES-v1                4
COLD                   4
FREQ-LRU-REPAIR        3
```

Fresh holdout, 72 regimes:

```text
ADES-V2              28
COLD                  27
ADES-v1               17
```

The result therefore materially improves the PR49 phase behavior: ADES-v2 is the most frequent winner in both the development and fresh holdout matrices. It does **not** establish universal dominance.

## Remaining development failure mode: delayed churn promotion

All six statistically material development regressions are against B3L on `churn` workloads. They occur on NY and grid regimes where resident state is eventually profitable.

The ADES-v2 rows still spend substantial early work cold before stabilizing. Across those failing cells the frozen run records examples such as:

```text
NY churn:        8–20 cold queries before four residents stabilize
Grid churn:     11–32 cold queries before three/four residents stabilize
```

There are no corresponding eviction storms in these cells. The remaining deficit is primarily **late materialization**, not capacity thrashing.

This is consistent with the finite economic horizon and the requirement for observed reuse before promotion: B3L pays the resident construction immediately, while ADES-v2 waits for evidence that the build will amortize.

This is a valid remaining research/engineering target; it must not be repaired by relabeling the already-observed development cells as fresh evidence.

## Fresh holdout contradiction: cold-path predictor overhead

The single statistically material holdout regression is:

```text
graph:           syn-clustered-50000-c100-d6-v1
family:          hot-pool
update interval: 1/2
update mode:     repeated-edge
budget:          16 MiB
comparator:      ADES-v1
comparator/V2:   0.808387911
95% CI:          0.707710862–0.923387008
```

A ratio below 1 means ADES-v1 is faster. In this cell ADES-v2 executes all 60 queries cold for every seed and performs zero promotions, zero evictions, and zero resident rebuilds. The regression is therefore not caused by a bad resident-state choice. It is **controller/predictor overhead on a workload that remains entirely cold**.

This contradiction is preserved rather than tuned away.

## Permanent evidence

The first frozen development + holdout result is permanently committed under:

```text
evidence/pr50-economic/3fb5f57a19c477742c0a87bac6f901f53b37d732/
```

Browsable files include:

- `measurements.csv`
- `summary.csv`
- `confidence.csv`
- `ANALYSIS.md`
- `decision.json`
- `manifest.json`
- `README.md`
- `SHA256SUMS`

`compact-evidence.tgz` additionally preserves every canonical trace and exact oracle bundle used by both matrices.

## PR50 conclusion

**PR50 is a successful algorithmic improvement, but the strongest target is only partially achieved.**

ADES-v2 substantially changes the phase diagram in the intended direction and is the most frequent fastest profile in the fresh holdout. Exactness and equal-byte enforcement remain intact. However:

- the 36-regime development matrix still contains six material B3L/churn losses caused primarily by delayed promotion;
- the 72-regime fresh holdout contains one material ADES-v1 loss caused by predictor overhead on a fully cold workload;
- therefore the claim "ADES-v2 wins everywhere" is contradicted by the frozen evidence.

Any next controller iteration must treat this first holdout as consumed evidence. New tuning should use these failures as hypotheses, then be evaluated on a **new holdout** with new graph/workload/seed combinations.
