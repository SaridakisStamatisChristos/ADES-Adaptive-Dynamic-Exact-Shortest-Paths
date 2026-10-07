# Benchmark protocol

ADES benchmark results are accepted only when every query result for every baseline matches a fresh-Dijkstra oracle on the identical deterministic trace.

## Baselines

- **B0** — fresh exact Dijkstra per query.
- **B1** — fresh exact bidirectional Dijkstra per query.
- **B2** — always-resident exact SSSP; every weight change rebuilds every materialized source.
- **B3** — always-resident exact SSSP; local dynamic repair with exact rebuild fallback.
- **B4** — full ADES: cold bidirectional queries, cost-aware admission, bounded resident cache, local repair, adaptive repair/rebuild policy.

Trace generation and oracle construction occur before the timed baseline section. A baseline that disagrees with the oracle exits with failure; its performance number is invalid.

## Reproducibility metadata

The run_matrix script records UTC time, commit SHA, OS/kernel, CPU model, total RAM, compiler, CMake version, build type, declared thread count, graph path, seed, operation count, repetition, query count, elapsed nanoseconds, per-process peak RSS, and the material B4 configuration.

Every timed B0–B4 measurement runs in a separate process. `run_matrix.sh` records `/usr/bin/time` peak RSS for that process, preventing both RSS and allocator/cache state from contaminating later baselines. Oracle construction occurs inside the process but outside the timed engine section.

## Required workload families

Seeded mixed synthetic traces; NY DIMACS distance and travel-time road graphs; local/geographic and broad/cross-region patterns; rotating semi-hot sources; update storms; catastrophic selected-SPT cuts; and repair-controller ablations.

Performance claims require repetitions and distribution statistics. Exactness failure invalidates the corresponding performance run.


## Repair-controller ablation

Controller evaluation must report, for each selected-parent increase repair, the measured discovery vertices, SPT tree edges, incoming-boundary scans, restricted-subgraph scans, and priority-queue pops. The adaptive policy learns three EWMAs: full rebuild time, repair nanoseconds per aggregate work unit, and aggregate work expansion per discovered vertex. The discovery budget is derived from the predicted total repair cost relative to `gamma * rebuild_cost`, then clamped by independent hard vertex/tree-edge ceilings.

Ablations must compare at least: fixed discovery ceiling, legacy vertex-only calibration, and work-aware calibration. Catastrophic-cut experiments must demonstrate early abort before state mutation; small-cut experiments must demonstrate that profitable repairs are not systematically forced into rebuilds. Controller decisions affect performance only: every abort falls back to exact full Dijkstra.
