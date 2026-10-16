# PR71 — SNAP email-Eu-core external-topology freeze

## Purpose

Freeze and independently validate a real non-road directed topology before any new ADES-vs-GraphBolt comparative timing is permitted.

This PR is **dataset/provenance qualification only**. It does not run either comparator and cannot support a performance claim.

## External source

Primary source: Stanford Network Analysis Project (SNAP), `email-Eu-core`.

Direct topology artifact:

`https://snap.stanford.edu/data/email-Eu-core.txt.gz`

SNAP describes the graph as a directed email network from a large European research institution and publishes the following structural statistics:

- nodes: 1,005
- directed edges: 25,571
- nodes in largest weakly connected component: 986
- edges in largest weakly connected component: 25,552
- nodes in largest strongly connected component: 803
- edges in largest strongly connected component: 24,729
- diameter: 7

The topology has no native edge weights in the file used here. Any later weighted shortest-path experiment must therefore label its weights as **synthetic deterministic weights over a real external topology**; it must not imply measured email latency, cost, or distance.

## Preserved discovery failure and parser correction

The first topology-discovery run on head `d1234e25c997423860138c8c0da364504c2db487` is preserved as a rejected run:

- workflow run: `38841406553`
- job: `116782114193`
- artifact: `12021332911`
- artifact ZIP SHA-256: `d8a8e6ad801dc46c03547a1690eedd473694bef3a63e9ba6cfba15c3c7fc108e`
- failure: the preregistered parser rejected the first observed self-loop, `(54,54)`, at source row 45
- no comparative timing was executed

Inspection of the preserved raw artifact showed that the source file contains:

- exactly 25,571 rows and 25,571 unique directed edges;
- exactly 1,005 vertices with IDs `0..1004`;
- **642 self-loops**;
- zero duplicate directed edges.

The self-loops are part of the published SNAP artifact and are included in SNAP's published edge/component counts. Preserving them reproduces the published component statistics exactly. Removing them leaves component membership unchanged but changes the internal-edge counts, so rejecting or silently deleting them would make the frozen topology less source-faithful.

Accordingly, the rejected first-run assumption `self loops are invalid` is explicitly falsified. The repaired qualification rule preserves self-loops exactly as published, counts them, and continues to reject duplicate directed edges, negative IDs, malformed rows, and structural mismatches. This correction does not change SNAP's published expected node/edge/WCC/SCC values and does not use performance outcomes.

The preserved discovery artifact also fixes the exact source identity for the canonical rerun:

- raw gzip SHA-256: `4b47acdb80197b085fe63c819c357ae488131ee904ed93d1b219a68b0f9e245f`
- canonical lexicographically sorted full edge-list SHA-256, **including self-loops**: `f0cfcb0a49f8d12fa6a92061a93d07becd8808185494e9d73d4f6695809e9d5e`
- frozen observed self-loop count: `642`

These values are hard gates for the canonical PR71 rerun. They may not be changed to accommodate a later download.

## Canonical freeze procedure

The PR71 workflow must:

1. download the exact direct SNAP gzip URL above;
2. require raw gzip SHA-256 `4b47acdb80197b085fe63c819c357ae488131ee904ed93d1b219a68b0f9e245f`;
3. parse only non-comment edge rows containing exactly two integer node IDs;
4. preserve edge direction and self-loops exactly as published;
5. reject duplicate directed edges, negative IDs, malformed rows, or any structural count mismatch;
6. verify the vertex ID range is exactly `0..1004` and the unique vertex count is 1,005;
7. verify the directed edge count is exactly 25,571 and the self-loop count is exactly 642;
8. independently compute weakly connected and strongly connected component statistics and compare them against SNAP's published values above;
9. construct a canonical normalized edge list sorted lexicographically by `(u,v)`, one `u v` pair per line with Unix newline, retaining self-loops;
10. require canonical edge-list SHA-256 `f0cfcb0a49f8d12fa6a92061a93d07becd8808185494e9d73d4f6695809e9d5e`;
11. emit a JSON manifest containing source identity, both hashes, parser rules, self-loop count, structural checks, and disposition;
12. upload the raw gzip, canonical edge list, manifest, and provenance as a retained evidence artifact.

## Admission rule

PR71 passes only if every published structural invariant matches, the frozen observed self-loop count matches, and both exact hashes match the discovery artifact.

Accepted disposition:

`QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_EMAIL_EU_CORE`

Otherwise:

`REJECTED_EXTERNAL_TOPOLOGY_FREEZE`

A rejection is an admissible outcome. Neither SNAP's published structural values nor the frozen hashes/self-loop count may be modified in response to a failing canonical rerun.

## Claim boundary

A green canonical PR71 establishes only that one exact external directed topology artifact was reproducibly fetched, normalized, hashed, and structurally validated.

It does **not** establish:

- shortest-path correctness;
- GraphBolt or ADES performance;
- superiority of either system;
- native weighted semantics;
- external generalization beyond this topology;
- universal or SOTA status.

## Next gate

Only after the repaired hash-pinned PR71 rerun succeeds may a separate performance branch preregister deterministic positive weights, a source-fidelity policy for semantically inert positive self-loops, fixed sources, update schedule, distance-bound checks, correctness oracles, timing rules, and statistical thresholds before comparative outcomes are observed.
