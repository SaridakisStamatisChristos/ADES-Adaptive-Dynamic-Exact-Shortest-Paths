# PR39 — Dynamic crossover evidence protocol

PR37 established that the matched-cap harness executes correctly, but its 10-query default did not actually execute an edge update when `update_every=10`. PR39 adds a deliberately dynamic evidence path without changing B2L, B3L, or B4 algorithm semantics.

## Trace instrumentation

`ades_phase` now accepts an optional final update-mode argument:

```text
ades_phase ... [cap] [random|alternating]
```

`random` is the default and preserves the legacy PR37 update-generation logic. `alternating` is an explicit benchmark mode that alternates genuine weight increases and decreases. For a requested decrease, a positive-weight edge is selected deterministically from the seeded starting edge if necessary; for an increase, an edge with sufficient numeric headroom is selected. The phase runner emits a machine-readable stderr record:

```text
TRACE_UPDATES mode=alternating total=N increases=I decreases=D unchanged=U
```

The existing phase CSV schema is unchanged.

## Harness validation

`tools/memory_crossover.py` now records the phase update count and direction counts in `measurements.csv`. With `--require-both-update-directions`, an invocation is rejected unless it contains at least one actual increase and one actual decrease. It also rejects mismatches between the CSV update count and the trace diagnostic, and rejects trace-signature disagreement among B2L/B3L/B4 or repetitions within a matched cell.

The dynamic workflow uses `alternating` mode so direction coverage is guaranteed by construction rather than left to a lucky random seed.

## Default experiment

Workflow: `.github/workflows/dynamic-crossover.yml`

Default matrix:

- New York DIMACS distance graph.
- 100 queries per trace.
- 2 repetitions.
- Capacities: 2 and 8 resident sources.
- Seeds: 7 and 17.
- Source families: `uniform`, `single-hot`, `hot-pool`.
- Update intervals: every 5 and every 10 queries.
- Baselines: B2L, B3L, B4.
- Separate process per baseline invocation.
- Independent Dijkstra oracle checks every query result.
- GNU `time` records per-process peak RSS.
- Baseline order rotates by cell/repetition.

At 100 queries, `update_every=5` produces 19 updates (10 increases, 9 decreases) and `update_every=10` produces 9 updates (5 increases, 4 decreases) under alternating mode. The full default matrix is 24 matched cells × 2 repetitions × 3 algorithms = **144 invocations**.

## Generated evidence

The workflow uploads:

- `manifest.json` — commit, graph SHA-256, platform and complete configuration.
- `measurements.csv` — per-invocation runtime, peak RSS, update direction counts, exit status and algorithm counters.
- `summary.csv` — descriptive paired medians grouped by update mode, source family, update interval and capacity.
- `ANALYSIS.md` — human-readable descriptive summary.
- Per-invocation stdout, stderr and GNU-time files.

`tools/analyze_crossover.py` defines speedup as comparator algorithm time divided by B4 algorithm time, so values above 1 favor B4. Its RSS ratio is comparator peak RSS divided by B4 peak RSS, so values above 1 mean B4 used less measured peak RSS.

## Interpretation limits

This experiment matches **resident-source capacity**, not exact byte-level memory budgets. Peak RSS includes graph loading, trace generation, oracle execution, allocator effects and temporary allocations, while `algorithm_ns` times only the phase runner's algorithm-operation loop. The default matrix has two seeds and two repetitions, so results are descriptive rather than publication-grade inferential evidence. A successful run can establish exactness on the exercised traces and characterize dynamic crossover behavior; it cannot by itself establish a universal or state-of-the-art performance claim.

Original B2/B3, bounded B2L/B3L, and ADES B4 algorithm behavior is otherwise unchanged. No release or tag is part of PR39.
