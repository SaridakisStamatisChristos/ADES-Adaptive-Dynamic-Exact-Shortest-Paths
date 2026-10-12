# PR56 — Predictive Repair-vs-Rebuild Preflight

## Objective

Replace the refuted structural proxy `large affected SPT region => rebuild` with a causal economic decision made after read-only affected-region discovery and before semantic shortest-path mutation.

PR56 is stacked on the frozen PR55 radix-queue result. It does not rewrite PR54/PR55 evidence and does not weaken exactness, online semantics, or persistent-state accounting.

## Baseline problem

The historical work-aware controller converts measured rebuild time and measured repair nanoseconds per work unit into a target repair-work budget. It then translates that target back into discovery vertex/tree-edge limits using learned average work-per-vertex ratios.

Two problems follow:

1. a structurally large but cheap affected region may be rejected before its actual degree geometry is known;
2. the previous `max_total_work` checks could fire after distances/parents had already been invalidated, forcing an exact rebuild after paying partial repair work.

The archived controller experiment already showed that large affected regions can be cheaper to repair than to abort and rebuild.

## PR56 mechanism

### 1. Read-only discovery uses only hard safety ceilings

The work-aware controller no longer derives the discovery vertex/tree limits from an average work-per-vertex proxy. Discovery may continue up to the existing hard safety ceilings.

### 2. Degree-aware preflight before mutation

After the affected selected-SPT subtree is known, PR56 estimates remaining repair work using information available causally at that point:

- affected vertices;
- selected-tree edges discovered;
- exact sum of incoming adjacency sizes for affected vertices;
- exact sum of outgoing adjacency sizes for affected vertices;
- one useful priority-queue pop per affected vertex as the causal baseline.

Computing adjacency sizes is O(1) per affected vertex on the PR54 CSR representation and does not scan the edge payloads.

The controller's learned rebuild-time / repair-cost ratio supplies the maximum economical work budget. If the planned repair exceeds it, PR56 returns `RebuildRequired` before changing any semantic SSSP state.

### 3. Commit-to-finish after acceptance

If preflight accepts local repair, PR56 removes mid-repair economic aborts. Boundary reconstruction and restricted Dijkstra run to completion. Actual work is still measured and fed back into the controller.

This prevents the dominated sequence:

`partial semantic repair -> economic abort -> full rebuild`.

Correctness remains protected by the exact rebuild path when preflight rejects and by the unchanged local-repair theorem when it accepts.

## Cold-start behavior

Until both a rebuild-time estimate and a repair nanoseconds/work estimate exist, the work-aware controller permits repair up to the hard safety ceilings. This obtains an unbiased repair sample rather than forcing a synthetic cold-start cutoff. Rebuild cost is already observed when resident SSSP state is initially constructed.

## Verification

`tests/pr56_repair_preflight.cpp` checks:

- economic rejection is read-only with respect to distances, selected parents, and SPT links;
- an accepted repair finishes even when actual queue work exceeds the preflight estimate because of stale duplicate queue entries;
- repaired distances equal fresh Dijkstra;
- learned work-aware budgets retain hard discovery ceilings and express the economic decision only through work units.

The PR workflow additionally runs the complete CTest suite and a matched PR55-vs-PR56 NY controller experiment with identical deterministic traces, three independent seeds, two reversed-order repetitions per seed, small and catastrophic affected-region regimes, and preserved negative/inconclusive evidence.

## Claim boundary

PR56 is an engineering/controller experiment. A speedup is not assumed. Any regression or inconclusive result is retained and the branch remains independently rejectable without altering PR55.
