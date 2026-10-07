# Correctness obligations

ADES is exact by construction only if every execution path preserves or restores the resident single-source shortest-path invariant.

The complete algorithm-level statement, pseudocode, proof sketch, and complexity model live in [`docs/ALGORITHM.md`](ALGORITHM.md). This file is the compact engineering checklist used to keep implementation and tests aligned with that specification.

## Core invariants

1. **Stable topology and edge identity.** Edge endpoints and edge IDs do not change; only nonnegative integer weights change.
2. **Overflow-safe relaxation.** Saturating arithmetic represents unsupported finite sums as infinity rather than wrapping.
3. **Exact resident labels.** After every completed update, each resident source `s` satisfies `dist[v] = delta_G(s,v)` for every vertex.
4. **Parent witness consistency.** Each selected parent edge of a reachable non-source vertex realizes its current distance, and the intrusive child/sibling representation agrees with the parent relation.
5. **Strict-improvement reparenting.** Normal relaxation changes selected parent only on strict distance improvement; equal-distance alternatives do not trigger reparenting.
6. **Read-only semantic discovery.** Selected-parent increase discovery may mutate scratch epoch marks but must not change semantic shortest-path state before the affected region is accepted.
7. **Exact fallback.** Any case not safely handled locally falls back to fresh exact Dijkstra.
8. **Policy independence.** Admission, eviction, cooldown, and repair-budget decisions may change cost but never semantic answers.

## Implemented update obligations

### Weight decrease

For changed edge `(u,v)` with new lower weight:

- filter safely if `u` is unreachable from the resident source;
- filter safely if `dist[u] + new_weight >= dist[v]`;
- otherwise seed a strict-improvement Dijkstra propagation at `v` and continue until no label improves.

Tests must cover propagation beyond `v`, zero weights, equal-distance alternatives, disconnected regions, parallel edges where supported, repeated decreases, and saturation behavior.

### Weight increase

The implementation reasons from the old edge weight and old exact labels:

- if the old edge is not tight, filter it;
- if it is tight but not the selected parent into its head, conservatively request a full rebuild;
- if it is the selected parent, discover the selected-tree subtree rooted at the head;
- if discovery exceeds the current budget, abort before semantic mutation and rebuild;
- otherwise invalidate that subtree, seed it from all finite incoming boundary edges, and run Dijkstra restricted to the affected region.

Tests must cover selected-parent invalidation, tight non-parent alternatives, zero-weight/equal-distance structure, early-abort read-only behavior, repeated reparenting, epoch reuse/wrap, and exact rebuild after abort.

## Controller obligations

The repair controller is not part of the correctness argument. It may only choose a discovery budget inside configured hard ceilings. Any budget may cause an earlier exact rebuild, but must never cause an approximate answer.

Current NY evidence refutes the general economic rule that a large affected subtree necessarily makes rebuild cheaper than completing repair. Future controller work should therefore compare estimated remaining repair cost with rebuild cost rather than relying on structural size alone.

## Validation obligation

A performance result is invalid if exactness fails. Differential tests against fresh Dijkstra, adversarial regression cases, and deterministic trace replay therefore dominate benchmark claims.
