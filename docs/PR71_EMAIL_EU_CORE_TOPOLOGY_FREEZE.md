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

## Freeze procedure

The PR71 workflow must:

1. download the exact direct SNAP gzip URL above;
2. record SHA-256 of the received gzip bytes;
3. parse only non-comment edge rows containing exactly two integer node IDs;
4. preserve edge direction exactly as published;
5. reject self-loops, duplicate directed edges, negative IDs, malformed rows, or any structural count mismatch;
6. verify the vertex ID range is exactly `0..1004` and the unique vertex count is 1,005;
7. verify the directed edge count is exactly 25,571;
8. independently compute weakly connected and strongly connected component statistics and compare them against SNAP's published values above;
9. construct a canonical normalized edge list sorted lexicographically by `(u,v)`, one `u v` pair per line with Unix newline;
10. record SHA-256 of that canonical normalized edge list;
11. emit a JSON manifest containing source identity, both hashes, parser rules, structural checks, and disposition;
12. upload the raw gzip, canonical edge list, manifest, and provenance as a retained evidence artifact.

## Admission rule

PR71 passes only if every structural invariant matches the preregistered SNAP values and both hashes are recorded.

Accepted disposition:

`QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_EMAIL_EU_CORE`

Otherwise:

`REJECTED_EXTERNAL_TOPOLOGY_FREEZE`

A rejection is an admissible outcome and must not be repaired by changing expected structural values after observing the download.

## Claim boundary

A green PR71 establishes only that one exact external directed topology artifact was reproducibly fetched, normalized, hashed, and structurally validated.

It does **not** establish:

- shortest-path correctness;
- GraphBolt or ADES performance;
- superiority of either system;
- native weighted semantics;
- external generalization beyond this topology;
- universal or SOTA status.

## Next gate

Only after PR71 has a frozen canonical topology SHA-256 may a separate performance branch preregister deterministic positive weights, fixed sources, update schedule, distance-bound checks, correctness oracles, timing rules, and statistical thresholds before comparative outcomes are observed.
