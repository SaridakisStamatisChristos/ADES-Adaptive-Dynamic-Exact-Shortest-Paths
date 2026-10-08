# Benchmark protocol

ADES benchmark results are accepted only when every query result for every baseline matches a fresh-Dijkstra oracle on the identical deterministic trace.

## Publication-contract binding

Publication-facing experiments created after PR41 are governed by `docs/PUBLICATION_CONTRACT.md`.

Each such experiment specification, manifest, or archived evidence package must identify the publication claim IDs it addresses (for example `C1`, `C4`, or `C-SOTA`). An experiment without a declared claim mapping is exploratory and cannot later become central confirmatory evidence without an explicit protocol revision.

The publication contract controls scientific scope, claim boundaries, metric meanings, fairness rules, and evidence interpretation. This benchmark protocol controls lower-level execution mechanics. If the two documents appear inconsistent, the stricter evidence requirement applies until the inconsistency is resolved explicitly.

## Baselines

- **B0** — fresh exact Dijkstra per query.
- **B1** — fresh exact bidirectional Dijkstra per query.
- **B2** — always-resident exact SSSP; every weight change rebuilds every materialized source.
- **B3** — always-resident exact SSSP; local dynamic repair with exact rebuild fallback.
- **B4** — full ADES: cold bidirectional queries, cost-aware admission, bounded resident cache, local repair, adaptive repair/rebuild policy.

Trace generation and oracle construction occur before the timed baseline section. A baseline that disagrees with the oracle exits with failure; its performance number is invalid.

## Canonical trace identity and replay

Publication-facing traces use one canonical textual operation grammar:

```text
QUERY source target
UPDATE edge_id old_weight new_weight
```

Each record is terminated by `\n`; integers are unsigned decimal without decoration. The SHA-256 digest is computed over the complete canonical operation byte stream, in execution order.

Every execution records:

```text
trace_sha256
query_count
update_count
increase_count
decrease_count
```

Equal-weight updates count toward `update_count` but neither direction count.

Before execution, the trace validator replays updates against a copy of the initial graph and requires every recorded `old_weight` to equal the graph state produced by all preceding operations. Out-of-range vertices/edges, malformed records, inconsistent old weights, or trace SHA mismatches invalidate the run.

`run_matrix.sh` generates one canonical trace once, writes it to a temporary replay file, and passes the same file to each isolated B0–B4 process through `ADES_TRACE_FILE`. Each process independently regenerates the expected deterministic trace, reloads the canonical replay file, validates it against the graph, and fails if the two SHA-256 digests differ. The reusable oracle bundle separately stores the same trace identity plus exact query answers and is rejected if its SHA differs from the replayed operation stream.

The phase/bounded-comparator runner reports the same canonical SHA-256 and operation counts for B1/B2/B3/B4/B2L/B3L. Controller-policy ablations likewise report SHA-256 identity and counts for fixed, vertex-only, and work-aware policies. Analysis tooling must reject a matched cell if comparator rows disagree on trace identity or operation counts.

The old 64-bit engineering fingerprint remains present only in archived pre-PR42 evidence. It is not sufficient trace identity for new publication-facing experiments.

## Workload Generator v2

PR46 freezes the versioned synthetic-workload generator used by later publication-facing workload sweeps. The complete semantics are specified in `docs/WORKLOAD_GENERATOR_V2_PR46.md`.

A PR46 workload artifact is identified by:

```text
graph_sha256
seed
config_sha256
trace_sha256
```

where `graph_sha256` hashes the exact input graph file bytes, `config_sha256` hashes the canonical v2 generator configuration, and `trace_sha256` hashes the canonical PR42 operation stream.

Synthetic publication traces generated after PR46 must preserve the complete workload configuration, including source family, update interval, update mode, perturbation magnitude, hot-source count, epoch length, locality percentage, and burst length. The same graph bytes + seed + canonical configuration must regenerate a byte-identical trace.

The frozen source-family axis is:

```text
uniform
single-hot
hot-pool
zipf
rotating-hot
churn
```

The frozen update-rate axis is:

```text
static (0), 1/2, 1/5, 1/10, 1/50, 1/100 updates per query
```

implemented as update intervals `0, 2, 5, 10, 50, 100`.

The frozen update-mode axis is:

```text
increase-only
decrease-only
balanced-random
strict-alternating
bursty
repeated-edge
```

The frozen perturbation bands are:

```text
small  = [1,3]
medium = [4,31]
large  = [32,255]
```

Workload generation must fail rather than silently weaken an infeasible requested update direction or magnitude. Negative and ADES-unfavorable cells produced by this expanded workload space remain admissible evidence and must not be filtered post hoc.

## Equal-byte persistent-state fairness

PR43 introduces the common publication memory-control mechanism required by claim `C3`.

The fairness metric is `accounted_algorithm_state_bytes`, a versioned logical model of algorithm-owned persistent adaptive state. It is deliberately distinct from `peak_rss`. The complete accounting boundary and formulas are frozen in `docs/EQUAL_BYTE_BUDGET_PR43.md`.

Publication-facing bounded comparisons after PR43 must record at least:

```text
persistent_state_budget_bytes
accounted_algorithm_state_bytes
peak_accounted_algorithm_state_bytes
accounting_version
peak_rss
```

For B2L, B3L and B4, setting:

```text
ADES_PERSISTENT_STATE_BUDGET_BYTES=<positive integer>
```

activates hard enforcement. In that mode the phase runner raises the source-count ceiling to `|V|`, making it nonbinding so the byte budget controls residency. A measured row is invalid if either current or peak accounted persistent state exceeds the declared budget.

Equal-byte matched cells must use the same configured budget and accounting version for every direct bounded comparator. `tools/equal_byte_crossover.py` rejects a cell if B2L/B3L/B4 disagree on byte budget, accounting version, SHA-256 trace identity, or operation counts, or if any comparator exceeds the hard budget.

Legacy equal-source-cap experiments remain valid engineering diagnostics but do **not** satisfy C3 and must not be relabeled as equal-byte publication evidence.

## Publication timing and telemetry

PR44 introduces `schema_version=2` for publication-facing phase rows. The schema definition and compatibility parser live in `tools/phase_schema.py`; semantic changes require a schema-version bump rather than silent column reuse.

For every comparator:

```text
algorithm_ns = query_ns + update_ns
```

The query timer covers only the comparator's `query` call; exactness comparison occurs after the timer stops. The update timer covers only the comparator's `update` call. Trace generation, validation, oracle construction, percentile calculation, CSV emission, and exactness comparison are outside `algorithm_ns`.

`oracle_ns` is recorded separately and never contributes to algorithm time.

Schema v2 records nearest-rank p50/p95 latencies for all operations and separately for queries and updates. Empty operation classes report `0`.

B4 also reports a disjoint query-domain partition:

```text
b4_query_time_ns =
    b4_cold_query_ns
  + b4_resident_query_ns
  + b4_query_policy_ns
  + b4_promotion_ns
  + b4_eviction_ns
```

and a disjoint update-domain partition:

```text
b4_update_time_ns =
    b4_graph_update_ns
  + b4_decrease_repair_ns
  + b4_increase_repair_ns
  + b4_rebuild_ns
  + b4_controller_ns
  + b4_update_policy_ns
```

Promotion-time Dijkstra construction is charged to `b4_promotion_ns`; Dijkstra fallback after a repair abort is charged to `b4_rebuild_ns`. They are not counted in both domains.

Every B4 row must reconcile exactly under both identities. Analysis tooling rejects a non-reconciling schema-v2 row. The detailed field semantics and CI acceptance rules are frozen in `docs/PUBLICATION_TELEMETRY_PR44.md`.

## Reproducibility metadata

The run_matrix script records UTC time, commit SHA, OS/kernel, CPU model, total RAM, compiler, CMake version, build type, declared thread count, graph path, seed, operation count, repetition, query/update counts, update-direction counts, trace SHA-256, elapsed nanoseconds, per-process peak RSS, and the material B4 configuration.

Every timed B0–B4 measurement runs in a separate process. Baseline order rotates deterministically by repetition to reduce systematic thermal/frequency order bias. `run_matrix.sh` records `/usr/bin/time` peak RSS for that process, preventing both RSS and allocator/cache state from contaminating later baselines. Trace generation and oracle construction occur outside the timed engine section.

## Required workload families

Publication-facing synthetic sweeps after PR46 must use Workload Generator v2 for the frozen source/update/magnitude axes above. Additional structured workloads remain required where relevant: NY DIMACS distance and travel-time road graphs; true 2-D grid-derived local/geographic and broad/cross-region patterns; update storms; catastrophic selected-SPT cuts; and repair-controller ablations.

Performance claims require repetitions and distribution statistics. Exactness failure invalidates the corresponding performance run.

## Repair-controller ablation

Controller evaluation must report, for each selected-parent increase repair, the measured discovery vertices, SPT tree edges, incoming-boundary scans, restricted-subgraph scans, and priority-queue pops. The work-aware policy learns full rebuild time, repair nanoseconds per aggregate work unit, aggregate work expansion per discovered vertex, and SPT-tree-edge expansion per discovered vertex. The legacy vertex-only ablation learns repair nanoseconds per discovered vertex and ignores the other repair-work dimensions. The discovery budget is derived from the predicted total repair cost relative to `gamma * rebuild_cost`, then clamped by independent hard vertex/tree-edge ceilings.

Ablations must compare at least: `fixed` discovery ceiling, legacy `vertex`-only calibration, and `work`-aware calibration through the same ADES code path. Reports must include cold/resident queries, promotions, rebuilds, filtered updates, decrease/increase repairs, repair aborts, elapsed time, and peak RSS. Random-source traces are not sufficient controller evidence when they produce few resident repairs; controller claims require a resident-hot/update-targeted workload that actually exercises selected-parent increase repair. Catastrophic-cut experiments must demonstrate early abort before state mutation; small-cut experiments must demonstrate that profitable repairs are not systematically forced into rebuilds. Controller decisions affect performance only: every abort falls back to exact full Dijkstra.

### Resident-targeted controller workload

`ades_controller_workload` first computes a deterministic update sequence with an independent Dijkstra SPT model, then replays that identical sequence against each controller policy. The benchmark forces one source resident before timing. Every round increases a selected current-SPT parent edge, then issues an exact query from that resident source and checks it against fresh Dijkstra.

- `small`: prefer SPT edges whose descendant subtree is at most max(8, n/1000), exercising profitable local repair.
- `catastrophic`: prefer SPT edges whose descendant subtree is at least n/4, exercising early repair abandonment/rebuild behavior.
- If a requested size class is absent, the generator deterministically falls back to a reachable SPT edge and the run must be interpreted from its observed repair/abort counters rather than its regime label alone.

Use `benchmarks/run_controller_ablation.sh` for isolated repeated fixed/vertex/work measurements with per-process RSS.
