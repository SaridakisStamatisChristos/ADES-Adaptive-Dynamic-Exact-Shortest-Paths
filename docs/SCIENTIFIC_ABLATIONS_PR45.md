# PR45 — Scientific Ablation Baselines

## Purpose

PR45 isolates which parts of the ADES policy are responsible for performance differences. The experiment is explicitly designed to distinguish gains from simple caching and dynamic repair from gains attributable to work-aware admission, update-debt-aware residency, and the full stabilizing policy.

**Claims addressed:** `C5`, `C6`, `C7`, `C9`.

PR45 is an ablation-enablement and validation stage. A favorable result is not required for the PR to pass. Negative, neutral, and contradictory results are valid evidence and must remain archived.

## Single-engine rule

All PR45 variants execute through the same `ADES` implementation. The variants are not separately written baseline classes.

This controls an important scientific confound: a performance difference must not be explainable merely by one comparator having a more optimized implementation, different data layout, different exactness path, or different timing harness.

The policy surface is explicit in `Config`:

```text
AdmissionPolicy
EvictionPolicy
MaintenancePolicy
admission_hysteresis_enabled
cooldown_enabled
RepairPolicy
```

The default configuration preserves the pre-PR45 full ADES behavior.

## Frozen ablation ladder

The six profiles are:

### 1. COLD

```text
admission = disabled
resident capacity = 0
eviction = irrelevant / LRU selected
maintenance = full rebuild selected but unreachable
hysteresis = disabled
cooldown = disabled
```

Every query executes exact cold bidirectional search. No probation, resident SSSP, cooldown, or adaptive state is retained.

Purpose: lower-complexity exact search control inside the same ADES query engine.

### 2. FREQ-LRU-REBUILD

```text
admission = frequency only
eviction = LRU
maintenance = full rebuild on every changed update
hysteresis = disabled
cooldown = disabled
```

A source becomes eligible after the fixed probation-query count. Cold-work measurements do not affect eligibility.

Purpose: measure the benefit/cost of bounded repeated-source caching without dynamic repair.

### 3. FREQ-LRU-REPAIR

```text
admission = frequency only
eviction = LRU
maintenance = local exact repair with exact rebuild fallback
hysteresis = disabled
cooldown = disabled
```

Purpose: isolate the incremental value of dynamic repair beyond simple frequency + LRU residency.

This is the principal simple-policy comparator for claim `C7`.

### 4. WORK-LRU-REPAIR

```text
admission = work aware
eviction = LRU
maintenance = local exact repair with exact rebuild fallback
hysteresis = disabled
cooldown = disabled
```

Work-aware admission retains the same minimum query-frequency gate but additionally requires accumulated measured cold-search edge scans to justify a resident SSSP build.

Purpose: isolate work-aware admission from frequency-only admission (`C5`).

### 5. WORK-DEBT-REPAIR

```text
admission = work aware
eviction = debt aware
maintenance = local exact repair with exact rebuild fallback
hysteresis = disabled
cooldown = disabled
```

Debt-aware eviction uses the existing ADES resident score, which combines source hits, recency/age, and accumulated update debt.

Purpose: isolate update-pressure-aware residency (`C6`).

### 6. B4 / full ADES

```text
admission = work aware
eviction = debt aware
maintenance = local exact repair with exact rebuild fallback
hysteresis = enabled
cooldown = enabled
```

Purpose: measure the complete policy against the simpler scientific controls, especially `FREQ-LRU-REPAIR` (`C7`).

## Important interpretation rule

The ladder is causal only in the limited ablation sense that adjacent profiles differ by a deliberately narrow policy feature. A runtime difference is empirical evidence about the complete changed mechanism under the tested workload; it is not automatically a universal causal theorem.

In particular:

- `FREQ-LRU-REBUILD -> FREQ-LRU-REPAIR` isolates local repair versus unconditional resident rebuilds.
- `FREQ-LRU-REPAIR -> WORK-LRU-REPAIR` isolates the work-aware admission gate.
- `WORK-LRU-REPAIR -> WORK-DEBT-REPAIR` isolates debt-aware eviction.
- `WORK-DEBT-REPAIR -> B4` isolates hysteresis + cooldown as the remaining full-policy stabilizers.

The direct `FREQ-LRU-REPAIR` versus `B4` comparison is the minimum controlled test for `C7`.

## Exactness and trace parity

`ades_scientific_ablations` generates one deterministic online operation stream, validates its canonical trace, computes one SHA-256 trace identity, constructs one independent fresh-Dijkstra oracle, and replays the identical operations through all six profiles.

Every measured query must equal the oracle answer. Any mismatch invalidates the cell.

The emitted rows reuse publication telemetry schema v2. Each row therefore records:

- trace identity and operation counts;
- oracle time separately from algorithm time;
- query/update totals and latency percentiles;
- ADES component timing reconciliation;
- adaptive mechanism counters;
- persistent-state accounting metrics.

Although schema-v2 field names retain the historical `b4_*` prefix, PR45 uses those fields for every ADES-engine ablation profile. They retain the same timing semantics.

## Memory fairness

PR45 does not supersede PR43.

When `ADES_PERSISTENT_STATE_BUDGET_BYTES` is set, every resident-capable profile is constrained by the same logical persistent-state accounting model and hard byte budget. COLD retains no adaptive state and therefore consumes zero accounted algorithm-owned persistent state.

For pure policy-isolation smoke tests, equal source capacity may be used. Publication-facing support for `C5`–`C7` must obey the frozen fair-budget rules from the publication contract wherever memory is part of the matched comparison.

## Machine-checkable invariants

`tools/validate_scientific_ablations.py` requires:

1. exactly one row for each frozen profile;
2. telemetry schema v2 for every row;
3. identical workload configuration and cryptographic trace identity;
4. timing reconciliation for every ADES-engine row;
5. COLD has zero resident queries, promotions, evictions, and accounted adaptive state;
6. non-full profiles never exercise cooldown blocks or hysteresis admission rejection;
7. `FREQ-LRU-REBUILD` never reports local-repair counters;
8. negative or neutral performance results do not fail validation.

The validator reports the direct:

```text
FREQ-LRU-REPAIR algorithm time / full ADES algorithm time
```

A ratio greater than `1` means full ADES was faster in that cell. A ratio below `1` means the simpler frequency + LRU + repair policy was faster. Neither outcome is filtered.

## CI acceptance

`.github/workflows/scientific-ablations.yml` must:

1. verify the committed NY graph;
2. build the repository and run the complete CTest suite;
3. execute the six-profile matched ablation harness;
4. validate the six rows and their scientific invariants;
5. archive raw rows, stderr telemetry, and the generated summary artifact.

CI success establishes that the ablation machinery is internally consistent and reproducible. It does not establish `C5`, `C6`, `C7`, or `C9`; those require the later multi-workload, multi-graph, repeated campaign.

## Acceptance criterion from the roadmap

PR45 is complete when the repository can directly compare full ADES with a frequency + LRU + dynamic-repair policy through the same implementation and benchmark path, while preserving exactness, trace parity, timing integrity, and negative evidence.
