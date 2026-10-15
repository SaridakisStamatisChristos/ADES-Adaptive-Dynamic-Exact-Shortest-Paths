# PR58 — NetworKit DynDijkstra qualification result

## Disposition

**INELIGIBLE for DIRECT comparison under frozen comparator contract v1.0.**

This is a result about the exact pinned implementation:

`networkit/networkit@ab420840dbb8a61fcb1ea0d89e5acc44fe66f2ab`

It is not a claim that all NetworKit releases, all dynamic Dijkstra implementations, or the underlying algorithmic literature are incorrect.

## Reproduction lineage

- PR: #58
- qualification workflow run: `38682581236`
- semantic-preflight job: `116250210171`
- uploaded artifact ID: `11948539417`
- artifact SHA-256: `75c25c888ff153c23c108833f8e892308545bd1650a253e47b7cc4c068c857c5`
- NetworKit pinned commit: `ab420840dbb8a61fcb1ea0d89e5acc44fe66f2ab`
- Boost/other external comparators are not part of this PR58 result.

The build path fetched the exact parent commit, recursively initialized the gitlink-pinned submodules, verified their recorded revisions, and built NetworKit 11.2.1 successfully from that source.

## Semantic preflight

The hardened v2 probe isolated each case in its own subprocess with a 60-second timeout. A case timeout is therefore preserved as comparator evidence instead of cancelling the whole CI job.

| Case | Outcome |
|---|---|
| simple directed increase/decrease | PASS |
| zero-weight / equal-distance predecessors | MISMATCH |
| asymmetric directed arcs | PASS |
| randomized directed mixed updates | TIMEOUT |

### Exactness counterexample

In the zero-weight/equal-distance case, after increasing edge `(2,3)` from weight `0` to `2`, the maintained dynamic result disagreed with a fresh NetworKit Dijkstra run on the same mutated graph:

- node 3: DynDijkstra `0`, fresh Dijkstra `1`
- node 4: DynDijkstra `1`, fresh Dijkstra `2`

The case later diverged on five nodes after increasing edge `(0,2)` from `0` to `3`, including source-side distances becoming effectively infinite while the fresh exact run remained finite.

### Termination counterexample

The randomized directed mixed case timed out after 60 seconds during step 16 while processing an increase of edge `(24,25)` from weight `2` to `6` on a 32-node graph. Earlier steps had completed normally.

The probe intentionally does not reinterpret that timeout as a performance result. For the frozen online-comparator contract it is a termination failure for the tested operation sequence.

## Contract consequence

DIRECT requires every frozen hard gate to be `PASS`. The empirical exactness failure alone is sufficient to reject this pinned implementation for DIRECT use. The termination failure independently prevents qualification of arbitrary online mixed-update processing.

Therefore later qualification work for this implementation was stopped rather than weakening the contract:

- arbitrary-source adapter: not reached
- canonical PR42 matched-trace adapter: not reached
- independent ADES publication oracle: not reached
- PR43 persistent-memory enforcement/accounting: not reached
- PR44 timing integration: not reached

The machine-readable comparator registry records the pinned NetworKit candidate as `INELIGIBLE` and preserves the counterexamples.

## What this changes

PR58 closes the strongest previously executable dynamic external candidate as a negative result. It does **not** establish C-SOTA. The external-comparator blocker remains.

The next defensible route is two-track:

1. qualify at least one independent, exact, reproducible external recomputation baseline under the full DIRECT contract, so the publication harness has a real external matched baseline rather than only internal controls; and
2. continue the search for a true external dynamic implementation separately, without modifying or repairing an external comparator until it passes.

A patched fork of NetworKit is not an independent replacement for this failed candidate and must not be used to erase the PR58 negative result.
