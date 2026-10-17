# PR44 — Publication-Grade Timing and Telemetry

## Purpose

PR44 establishes a versioned timing and mechanism-telemetry contract for publication-facing ADES experiments. The goal is not merely to report that one method is faster or slower, but to make the cause of a win, loss, or crossover inspectable without double counting time.

**Claims addressed:** `C1`, `C2`, `C4`, `C5`, `C6`, `C7`, `C9`.

This PR changes measurement and observability. It does not assert that any performance claim is established.

## Schema version

Publication phase rows emitted by `ades_phase` use:

```text
schema_version = 2
```

The schema is defined centrally in `tools/phase_schema.py`. A later semantic change to a timing field requires a schema-version bump. Archived schema-v1 rows remain distinguishable and may be read as legacy evidence; they must not be silently interpreted as schema v2.

## Top-level timing contract

For every measured comparator:

```text
algorithm_ns = query_ns + update_ns
```

`algorithm_ns` is therefore a constructed sum of disjoint timed operation domains, not a second overlapping wall timer.

The timed interval for a query includes only the comparator's `query` call. Exactness comparison against the precomputed oracle answer occurs after the query timer stops.

The timed interval for an update includes only the comparator's `update` call.

Trace generation, trace validation, percentile calculation, CSV formatting, exactness comparison, and oracle construction are excluded from `algorithm_ns`.

## Oracle timing

`oracle_ns` measures construction of the independent fresh-Dijkstra reference answers on the same operation stream. It is recorded explicitly but is never included in `algorithm_ns`, `query_ns`, or `update_ns`.

This separation prevents validation cost from being mistaken for algorithm cost while keeping the cost of reproducibility visible.

## Latency statistics

Schema v2 records exact per-operation timings in the benchmark harness and reports nearest-rank percentiles:

```text
operation_p50_ns
operation_p95_ns
query_p50_ns
query_p95_ns
update_p50_ns
update_p95_ns
```

If a class contains no operations, its percentile is reported as `0` rather than fabricated from another class.

Latency-vector maintenance and percentile sorting occur outside the timed algorithm intervals. The vectors belong to the benchmark harness, not the algorithm-state byte budget.

## B4 query-time partition

ADES/B4 additionally records a disjoint internal decomposition:

```text
b4_query_time_ns =
    b4_cold_query_ns
  + b4_resident_query_ns
  + b4_query_policy_ns
  + b4_promotion_ns
  + b4_eviction_ns
```

Meanings:

- `b4_cold_query_ns` — bidirectional exact cold-search execution only.
- `b4_resident_query_ns` — complete resident-hit query path.
- `b4_query_policy_ns` — non-overlapping residual query-side policy work, including probation/admission bookkeeping and memory-policy work not classified below.
- `b4_promotion_ns` — promotion-time SSSP construction and insertion. A promotion Dijkstra is charged here, not to update-side rebuild time.
- `b4_eviction_ns` — resident eviction and associated cooldown-state maintenance caused by promotion pressure.

The implementation rejects a row if these components do not exactly reconcile to `b4_query_time_ns`.

## B4 update-time partition

ADES/B4 records:

```text
b4_update_time_ns =
    b4_graph_update_ns
  + b4_decrease_repair_ns
  + b4_increase_repair_ns
  + b4_rebuild_ns
  + b4_controller_ns
  + b4_update_policy_ns
```

Meanings:

- `b4_graph_update_ns` — mutation of the graph edge weight.
- `b4_decrease_repair_ns` — exact decrease-repair execution.
- `b4_increase_repair_ns` — exact increase-repair execution, including filtered/aborted repair attempts but excluding controller decision time.
- `b4_rebuild_ns` — fresh-Dijkstra fallback after a repair abort. Promotion builds are not included here.
- `b4_controller_ns` — controller budget decisions and controller-model observations.
- `b4_update_policy_ns` — residual non-overlapping update-side policy/bookkeeping work.

The implementation rejects telemetry overlap: classified component time may not exceed its enclosing operation domain.

## Counters retained with timing

Schema v2 emits mechanism counters alongside the timing fields:

```text
cold_queries
resident_queries
promotions
evictions
rebuilds
repair_aborts
cooldown_blocks
admission_rejections
filtered_updates
decrease_repairs
increase_repairs
memory_budget_rejections
memory_metadata_prunes
```

These counters are required to interpret timing causally. A speedup with no promotions, for example, cannot be attributed to adaptive residency merely because the row came from B4.

## Memory metrics remain separate

PR44 does not redefine PR43 memory fairness. The following remain distinct:

- `accounted_algorithm_state_bytes` / `peak_accounted_algorithm_state_bytes` — logical persistent-state fairness metrics;
- process peak RSS — observational system memory;
- benchmark-harness latency vectors — measurement infrastructure, excluded from the logical algorithm-state budget.

## Reconciliation gates

Three levels of checks are applied:

1. **C++ unit/acceptance test** — `ades_telemetry_tests` exercises cold, resident, promotion, decrease-update and increase-update paths and requires exact B4 component conservation.
2. **Schema parser** — `tools/phase_schema.py` validates `algorithm_ns = query_ns + update_ns` and the B4 query/update component identities.
3. **NY CI gate** — `.github/workflows/publication-telemetry.yml` runs matched B1/B2L/B3L/B4 rows on the committed NY graph, requires schema v2, identical trace identity, top-level reconciliation, B4 reconciliation, and uploads the raw telemetry evidence.

Any reconciliation failure invalidates the row rather than being treated as a warning.

## Scientific interpretation

PR44 makes mechanism analysis possible; it does not itself prove a mechanism. Later campaign stages must use these fields to test hypotheses such as:

- whether avoided cold-query work outweighs promotion and maintenance cost (`C9`);
- whether admission or update-debt policies explain differences from simpler controls (`C5`–`C7`);
- whether observed wins survive equal-byte fairness and matched traces (`C2`–`C4`).

Negative and contradictory telemetry must remain in the evidence package under the publication contract.
