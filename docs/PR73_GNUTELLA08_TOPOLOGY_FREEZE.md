# PR73 — SNAP p2p-Gnutella08 external-topology discovery and freeze

## Purpose

Freeze and independently validate a **second real external directed topology from a distinct domain class** before any new ADES-vs-GraphBolt timing is permitted.

PR73 is dataset/provenance work only. It must not build or run ADES or GraphBolt and cannot support a performance claim.

## Why this topology

Source: Stanford Network Analysis Project (SNAP), `p2p-Gnutella08`.

Direct artifact:

`https://snap.stanford.edu/data/p2p-Gnutella08.txt.gz`

SNAP describes this as a directed snapshot of the Gnutella peer-to-peer file-sharing network from August 8, 2002. Nodes are hosts and edges are connections between hosts.

Published structural invariants:

- nodes: 6,301
- directed edges: 20,777
- largest WCC: 6,299 nodes / 20,776 edges
- largest SCC: 2,068 nodes / 9,313 edges
- diameter: 9

This domain and topology are distinct from PR71/72's institutional email network.

## Two-stage source freeze

Exact compressed bytes and source-representation details are not assumed in advance.

### Stage A — discovery-only run

The first PR73 run may only:

1. download the exact direct SNAP gzip URL;
2. hash the received gzip bytes;
3. parse all non-comment rows as two integer endpoint IDs;
4. reject malformed rows or negative IDs;
5. record raw data-row count, unique directed-edge count, duplicate-row count, self-loop count, unique node count, and observed min/max IDs;
6. compute graph structural statistics over the set of unique directed edges, preserving self-loops if present;
7. hard-fail unless the unique-node count, unique-edge count, largest-WCC node/edge counts, and largest-SCC node/edge counts match SNAP's published values above;
8. emit a canonical lexicographically sorted **unique directed edge list**, preserving any unique self-loops;
9. record the canonical edge-list SHA-256;
10. upload raw/canonical artifacts and a manifest.

Stage A is **not canonical evidence**, even if structural checks pass. It only discovers exact byte/content identity and representation details. No later performance experiment may cite a Stage-A-only topology.

### Stage B — canonical hash-pinned rerun

After a successful Stage A, a follow-up commit on the same PR must hard-pin, without changing the published structural invariants:

- raw gzip SHA-256;
- canonical unique-edge-list SHA-256;
- observed raw data-row count;
- observed duplicate-row count;
- observed self-loop count;
- observed min/max node IDs.

The same validator must then rerun from a fresh download and require all pinned values exactly. Only that Stage-B run may receive the disposition:

`QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_P2P_GNUTELLA08`

Otherwise the topology remains:

`NOT_CANONICALLY_FROZEN`

## Canonical graph semantics

The canonical graph is the set of unique directed endpoint pairs from the source artifact, sorted lexicographically and retaining unique self-loops if any.

If Stage A reveals duplicate source rows, their existence is recorded and frozen, but duplicates do not create parallel graph arcs because SNAP's published edge count and component-edge counts are validated against the unique directed-edge set. Any such normalization must remain explicit in the manifest.

No endpoint direction is changed. No reverse arc is introduced. No component extraction is applied.

## Claim boundary

A successful PR73 establishes only reproducible identity and structural qualification of one exact real Gnutella topology.

It does **not** establish:

- weighted semantics;
- shortest-path correctness;
- ADES or GraphBolt performance;
- superiority;
- cross-topology generalization;
- universal or SOTA status.

## Next gate

Only after Stage B succeeds may PR74 separately preregister deterministic positive synthetic weights, fixed source selection, meaningful non-self-loop weight replacements, GraphBolt's finite-distance bound, correctness oracles, timing boundaries, repetitions, and an outcome-neutral statistical decision rule.
