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

## Reproducibility metadata

The run_matrix script records UTC time, commit SHA, OS/kernel, CPU model, total RAM, compiler, CMake version, build type, declared thread count, graph path, seed, operation count, repetition, query/update counts, update-direction counts, trace SHA-256, elapsed nanoseconds, per-process peak RSS, and the material B4 configuration.

Every timed B0–B4 measurement runs in a separate process. Baseline order rotates deterministically by repetition to reduce systematic thermal/frequency order bias. `run_matrix.sh` records `/usr/bin/time` peak RSS for that process, preventing both RSS and allocator/cache state from contaminating later baselines. Trace generation and oracle construction occur outside the timed engine section.

## Required workload families

Seeded mixed synthetic traces; NY DIMACS distance and travel-time road graphs; true 2-D grid-derived local/geographic and broad/cross-region patterns with the same 25% update process as mixed traces; rotating semi-hot sources; update storms; catastrophic selected-SPT cuts; and repair-controller ablations.

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
