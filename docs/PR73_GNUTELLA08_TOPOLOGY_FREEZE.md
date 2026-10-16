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

Exact compressed bytes and source-representation details were not assumed before Stage A.

### Stage A — completed discovery-only run

The first executable PR73 head was:

`ec8b32bee5838c32638d759df0418f6e9b4b3d3e`

Stage-A evidence:

- run: `38842626427`
- job: `116785947358`
- artifact: `12021379965`
- artifact ZIP SHA-256: `baf7cebcd86a2254b14f6f7485749a175194716d5461919c86fad6d859c0912d`
- conclusion: structural discovery passed
- comparative timing executed: **false**
- canonical evidence admitted: **false**

Stage A reproduced all SNAP-published graph invariants and discovered the following exact representation identity:

- raw gzip SHA-256: `cb3019062be21e55b03f19a73e88edf34693bafb68ae581621cc7a037fa604a9`
- canonical lexicographically sorted unique-directed-edge-list SHA-256: `4554021f32967c69b68ccf59f9d85b83b66a6c0413b721112a10741858fdf845`
- raw data rows: `20,777`
- unique directed edges: `20,777`
- duplicate rows: `0`
- self-loops: `0`
- unique nodes: `6,301`
- minimum node ID: `0`
- maximum node ID: `6300`
- largest WCC: `6,299` nodes / `20,776` edges
- largest SCC: `2,068` nodes / `9,313` edges

Stage A disposition remains deliberately noncanonical:

`DISCOVERY_STRUCTURALLY_VALID_NOT_CANONICALLY_FROZEN`

### Stage B — canonical hash-pinned rerun

The values discovered above are now frozen. The canonical workflow must fresh-download the same URL and require exactly:

- raw gzip SHA-256 `cb3019062be21e55b03f19a73e88edf34693bafb68ae581621cc7a037fa604a9`;
- canonical unique-edge-list SHA-256 `4554021f32967c69b68ccf59f9d85b83b66a6c0413b721112a10741858fdf845`;
- raw data rows `20777`;
- duplicate rows `0`;
- self-loops `0`;
- minimum node ID `0`;
- maximum node ID `6300`;
- all unchanged SNAP-published node/edge/WCC/SCC invariants above.

No later failure may be repaired by changing these pins to fit a new download.

Only a fresh Stage-B run satisfying every pinned representation value and published structural invariant may receive:

`QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_P2P_GNUTELLA08`

Otherwise the topology is rejected and remains unavailable for performance admission.

## Canonical graph semantics

The canonical graph is the set of unique directed endpoint pairs from the source artifact, sorted lexicographically.

Stage A established that this source representation already has exactly one row per directed edge, zero duplicate rows, and zero self-loops. Therefore the canonical unique-edge graph contains exactly the 20,777 source rows modulo lexicographic ordering only.

No endpoint direction is changed. No reverse arc is introduced. No component extraction is applied.

## Claim boundary

A successful Stage B establishes only reproducible identity and structural qualification of one exact real Gnutella topology.

It does **not** establish:

- weighted semantics;
- shortest-path correctness;
- ADES or GraphBolt performance;
- superiority;
- cross-topology generalization;
- universal or SOTA status.

## Next gate

Only after Stage B succeeds may PR74 separately preregister deterministic positive synthetic weights, fixed source selection, dynamic weight replacements, GraphBolt's finite-distance bound, correctness oracles, timing boundaries, repetitions, and an outcome-neutral statistical decision rule.
