# PR85 — Formal algorithm specification reconciliation

## Status

**DOCUMENTATION-ONLY SOURCE RECONCILIATION.**

PR85 repairs `docs/ALGORITHM.md` so the formal repository specification matches the frozen ADES candidate used in the publication-facing holdouts before a dedicated prior-art/novelty audit is attempted.

It does not modify ADES code, comparator code, benchmark seeds, timing semantics, memory accounting, Publication Contract v1.0, or the scoped practical-SOTA evidence decision.

## Claims addressed

```text
claim_ids: [C1, C3, C4, C-SOTA]
```

The purpose is descriptive fidelity, not claim expansion.

## Frozen implementation identity

Publication-facing ADES candidate:

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

Source blobs that define the reconciled mechanisms:

- `src/dynamic_repair.cpp`: `116f12376d14dc62fdd25177b1e2ecb8175d81b4`
- `src/shortest_paths.cpp`: `02619728bcdb8047d2fee14cab9125563934e47e`
- `include/ades/shortest_paths.hpp`: `7b1b195b80c927d60b2346711f1ab071c7fa3c71`

PR54 phase-change specification:

- `docs/PR54_CORE_ENGINE_PHASE_CHANGE.md`
- blob `5fa83a6a3cf10c96e7fec1241e5d7e766ff7f71b`

PR57 accepted mechanism:

- PR #57 — `Large-Repair-Gated Alternate-Witness Rescue`
- frozen v2 head `e2a583e2028c31a4b99788657c669db1c1503d7c`
- v1 eager probe rejected because a broad matrix found a supported regression;
- v2 accepted subject to final compatibility/fresh holdout.

## Reconciled defects

### D85-1 — tight non-selected-parent increases

Stale formal-spec text said a tight edge that was not `parent_edge[v]` requested a full rebuild.

The frozen candidate actually implements:

```cpp
if(s.parent_edge[old.to]!=id)return RepairResult::Filtered;
```

This matches PR54. The safety argument is witness-based: the increased edge occurs nowhere in the selected SPT, therefore every reachable vertex retains its complete selected shortest-path witness. An increase cannot create a shorter path, so all distances remain unchanged.

`docs/ALGORITHM.md` is corrected to state the O(1) filter and its proof obligation.

### D85-2 — repair-membership scratch

Stale formal-spec text described epoch-mark scratch as resident per-source state.

PR54 moved increase-repair membership marking to one reusable thread-local epoch workspace. `SSSPState` retains a compatibility `repair_mark` vector plus scalar `repair_epoch`, but fresh Dijkstra construction initializes the vector empty.

The formal spec is corrected to distinguish:

- semantic resident SSSP state;
- the scalar diagnostic/lifecycle epoch;
- shared transient thread-local membership scratch.

This is important for the accounting-v2 memory contract.

### D85-3 — PR57 alternate-witness rescue missing from formal spec

The frozen candidate contains a bounded rescue path for selected-parent increases:

- no probe for repairs below 256 discovered vertices;
- exactly one bounded scan after the trigger;
- at most 64 incoming arcs of the affected root are inspected;
- a tight predecessor with strictly smaller old distance is immediately safe under nonnegative weights;
- same-distance tight candidates are retained and accepted only after complete subtree discovery proves the predecessor is outside the affected subtree;
- failure continues into the ordinary exact repair/rebuild path;
- no persistent shortest-path DAG is added.

The formal spec now states this path, its correctness boundary, and its fixed bounded probe cost.

### D85-4 — queue implementation description

The frozen candidate uses `RadixQueue` in:

- full resident Dijkstra;
- bidirectional cold search;
- decrease propagation;
- restricted increase repair.

The spec now identifies radix/monotone queues as the actual implementation while retaining conservative architecture-level asymptotic descriptions rather than claiming a new worst-case bound.

## Novelty boundary

PR85 does **not** claim that any reconciled mechanism is novel.

In particular, the following remain unestablished until a separate literature audit:

1. dynamic SSSP repair after edge increases/decreases;
2. selected-SPT witness reasoning for unaffected edges;
3. alternate shortest-path witness rescue;
4. adaptive bounded residency of exact per-source SSSP states;
5. cold-work-aware source admission;
6. hit/age/update-debt-aware eviction;
7. adaptive repair-vs-rebuild budgeting;
8. the overall combination as a scholarly systems contribution.

The next R7 campaign must search and adversarially map prior art against these components separately. Absence of a found collision is not sufficient evidence of novelty.

## Acceptance rule

PR85 is accepted only if CI proves:

1. the frozen candidate source blobs match the identities above;
2. `docs/ALGORITHM.md` says tight non-selected-parent increases are filtered, not rebuilt;
3. the document contains the frozen 256-vertex / 64-incoming-arc witness-rescue constants;
4. shared thread-local repair-membership scratch is documented;
5. radix queue use is documented without claiming an asymptotic breakthrough;
6. novelty remains explicitly unestablished;
7. the PR changes no performance implementation, comparator, benchmark, registry, or historical evidence artifact.

Only after those gates pass is the formal specification suitable as the target description for PR86 prior-art/novelty analysis.