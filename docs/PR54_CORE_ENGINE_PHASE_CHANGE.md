# PR54 — Core Engine Phase Change

## Status

PR54 is a controlled core-engine optimization campaign. It does not modify the frozen V4/V5 experimental branches and it does not change the online problem contract: directed fixed-topology graphs, nonnegative integer weights, online increases/decreases, arbitrary query sources, exact answers, no future knowledge, and explicit persistent-state byte budgets.

Performance claims from the PR54 matched workflow are **development evidence only** until a fresh post-freeze holdout is run.

## Changes

### 1. Tight non-selected-parent increases are filtered

For resident source `s`, let `T` be the selected shortest-path witness tree encoded by `parent_edge`.

Before PR54, an increased edge `(u,v)` caused a full rebuild whenever it was tight but was not `parent_edge[v]`.

PR54 returns `Filtered` instead.

Proof obligation:

1. the changed edge is not selected as the parent of its head, therefore it occurs nowhere in `T`;
2. every reachable vertex retains its complete selected shortest-path witness in `T` after the increase;
3. increasing an edge weight cannot create a shorter path;
4. because an unchanged shortest witness remains for every vertex, no shortest distance can increase either.

Therefore all resident distances and selected-tree links remain exact and unchanged. The implementation performs no discovery or semantic mutation for this case.

Adversarial coverage includes equal shortest paths, parallel tight edges, zero-weight alternatives, read-only state comparison, and randomized differential testing against fresh Dijkstra.

### 2. Reusable exact cold-query workspace

The old bidirectional Dijkstra path allocated and initialized two `|V|` distance arrays to infinity for every cold query and constructed two fresh queue containers.

PR54 uses one thread-local workspace containing:

- forward/backward distance storage;
- forward/backward epoch marks;
- reusable vector-backed binary heaps.

A vertex label is logically infinity unless its mark equals the current query epoch. Epoch wrap clears marks before reuse. The search termination condition and relaxation rules are unchanged.

This is temporary execution scratch and is excluded from the logical persistent-state budget for every profile equally.

### 3. Shared increase-repair membership scratch

The old resident SSSP representation allocated one `uint32_t repair_mark[|V|]` vector per resident source.

Repair operations execute synchronously, so membership marks are not source-semantic persistent state. PR54 moves them to one reusable thread-local epoch workspace. `repair_epoch` remains as a resident diagnostic lifecycle counter; the compatibility `repair_mark` member is left empty in newly constructed states.

This removes four persistent bytes per vertex per resident without reducing semantic state.

### 4. Packed SPT identifiers

Vertices and stable edges already use a 32-bit identifier domain. PR54 changes:

- `parent_edge`;
- `first_child`;
- `next_sibling`;
- `prev_sibling`

from signed 64-bit identifiers to `uint32_t`, using `UINT32_MAX` (`kNoIndex`) as the explicit absent-link sentinel. `Graph::add_edge` rejects exhaustion of the reserved sentinel value.

The semantic resident payload becomes:

```text
Distance                8 bytes / vertex
parent_edge             4
first_child             4
next_sibling            4
prev_sibling            4
--------------------------------
total                   24 bytes / vertex
```

plus resident scalar/controller metadata.

PR43 logical accounting is versioned to `accounting_version = 2`; the reduced representation is therefore visible to matched equal-byte experiments rather than being hidden as extra capacity.

### 5. Frozen CSR adjacency for file-loaded graphs

Topology is fixed by the ADES problem contract. During DIMACS loading PR54 therefore builds normally, then freezes forward and reverse adjacency into contiguous CSR-like arc arrays with per-vertex offsets. The nested construction adjacency is released.

The stable edge table remains separate and mutable, so online weight updates do not rebuild topology. Programmatically constructed graphs remain mutable until `freeze_topology()` is explicitly invoked.

Tests compare adjacency order, full Dijkstra distances, bidirectional distances, post-freeze weight updates, and rejection of structural edge insertion after freeze.

## Exactness and fairness gates

PR54 requires all of the following before readiness:

- complete repository CTest suite green;
- PR54 adversarial/differential tests green;
- NY DIMACS validation green;
- equal-byte budget gate green under accounting v2;
- telemetry reconciliation green;
- workload trace identity/oracle validation green;
- matched main-vs-PR54 development benchmark completed without exactness or budget violation.

## Matched development benchmark

`.github/workflows/pr54-core-benchmark.yml` checks out both `main` and the PR candidate and executes their isolated `ades_pilot_profile` binaries against:

- identical generated graph bytes;
- identical canonical trace files;
- the same oracle bundle generated by the baseline full-Dijkstra implementation;
- identical byte budgets;
- three independent seeds;
- two repetitions per seed with baseline/candidate execution order reversed on the second repetition.

Profiles:

- `COLD` — isolates common point-to-point kernel effects;
- `FREQ-LRU-REPAIR` — exercises resident build/repair and representation effects without the predictive controller;
- `ADES-V3` — measures the currently strongest merged adaptive profile on the new common engine.

For each seed, repeat log-speedups are averaged first. The summary then reports geometric baseline/candidate speedup with a 95% t interval across the three independent seeds. No positive performance result is required for workflow success; regressions are retained as evidence.

## Deliberately deferred

PR54 does **not** tune V5 or introduce a new adaptive controller. After core costs stabilize, the next controller generation should evaluate:

- predicted remaining repair cost versus rebuild cost;
- safe alternative-parent fast paths;
- source-specific reuse/maintenance hazard estimates;
- confidence-gated resident replacement with amortization leases.

Those decisions should be calibrated on the post-PR54 cost landscape rather than the obsolete pre-PR54 engine.
