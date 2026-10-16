# PR74 — GraphBolt vs ADES on frozen SNAP p2p-Gnutella08 topology

## Status

**Preregistered performance contract.** Comparative timing is forbidden until this contract, the complete harness, and the CI workflow are committed on the branch.

PR74 tests whether the strong PR72 result survives a second real external directed topology from a different domain: a Gnutella peer-to-peer host network rather than institutional email.

## Frozen evidence lineage

Base/canonical topology:

- PR73 canonical head: `fa6637b57bef6c8b56aaf1f9ac65bd7fc7a26645`
- SNAP source: `p2p-Gnutella08`
- raw gzip SHA-256: `cb3019062be21e55b03f19a73e88edf34693bafb68ae581621cc7a037fa604a9`
- canonical unique-directed-edge-list SHA-256: `4554021f32967c69b68ccf59f9d85b83b66a6c0413b721112a10741858fdf845`
- vertices: 6,301, IDs `0..6300`
- directed edges: 20,777
- duplicate rows: 0
- self-loops: 0
- largest WCC: 6,299 nodes / 20,776 edges
- largest SCC: 2,068 nodes / 9,313 edges
- PR73 canonical run: `38842725427`
- PR73 canonical artifact: `12021871969`
- PR73 canonical artifact digest: `sha256:37f2e717cd66a6d8356bcc758463346269fd073e648ffcbadd19e6f3d45e3c49`

GraphBolt lineage remains exactly the PR69-qualified comparator:

- GraphBolt/DZiG commit `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- mimalloc v1.6.0 commit `04f1c3b1e23677ce03bd16137e73089abd552175`
- upstream OpenMP path, OMP4 for the primary performance run
- source-preserving `-include cstdint`
- only qualified source repair: `kickstarter-missing-return-v1`
- semantic scope: directed fixed-source positive-integer weighted streaming SSSP with finite distances `<65535`

ADES reuses the unchanged PR70 fixed-source runner and resident fixed-source maintenance semantics. No arbitrary-source cache admission, eviction, or cold-query policy is involved.

Any further GraphBolt source modification, ADES runner modification, or topology identity change invalidates this gate.

## Real topology, synthetic weights

The topology is real and externally sourced. The source is unweighted, so every edge receives a deterministic **synthetic positive integer weight** in `[1,30]` from a fixed SHA-256-derived function of `(weight_seed,u,v)`.

No native Gnutella latency, distance, bandwidth, cost, or duration is claimed.

## Frozen source selection

Sources are selected solely from the frozen topology, without performance information.

Take the sorted node IDs in PR73's 2,068-node largest SCC. Use zero-based positions:

- `floor(0.125 * 2068) = 258`
- `floor(0.375 * 2068) = 775`
- `floor(0.625 * 2068) = 1292`
- `floor(0.875 * 2068) = 1809`

The resulting hard-pinned source IDs are:

`[617, 2079, 3676, 5306]`

The harness must independently recompute the largest SCC and reject execution unless both the SCC size and the four selected IDs match exactly.

## Frozen scenarios

| scenario | source | weight seed |
|---|---:|---:|
| S0 | 617 | 7401 |
| S1 | 2079 | 7407 |
| S2 | 3676 | 7411 |
| S3 | 5306 | 7417 |

These are four robustness scenarios on **one Gnutella topology**. They are not independent topology replications.

## Frozen dynamic workload

For each scenario:

- 128 logical edge-weight replacements;
- all 20,777 edges are eligible because PR73 established zero self-loops;
- exact topology endpoints never change;
- replacements alternate decrease/increase;
- decrease: choose deterministically among currently weight>1 edges, delta uniformly in `[1,min(7,old-1)]`;
- increase: choose deterministically among all edges, delta uniformly in `[1,7]`;
- update RNG seed = `weight_seed XOR 0x74505550`;
- repeated edge selection is allowed and uses the current weight;
- GraphBolt represents one logical replacement as `delete(u,v,old)` then `add(u,v,new)` and is charged for both batches;
- ADES is charged for the logical weight replacement plus all dynamic repair/controller/fallback work.

No source, weight seed, update edge, delta, repetition count, or decision threshold may change after comparative outcomes are observed.

## Pre-execution finite-distance validation

Before committing the timing-capable workflow, the frozen workload was replayed only with a fresh exact Dijkstra oracle to test GraphBolt's semantic bound. No GraphBolt or ADES comparative timing was executed.

Maximum finite oracle distance over all logical states:

- S0: 217
- S1: 208
- S2: 212
- S3: 247

These values are far below 65,535. The harness must recompute and hard-fail if any finite distance reaches or exceeds 65,535.

## Correctness-before-performance gate

For every scenario before any measured pair:

1. verify the PR73 raw and canonical topology hashes;
2. independently recompute the frozen SCC/source rule;
3. replay all states with fresh Dijkstra and enforce the finite-distance bound;
4. run GraphBolt non-timed with full output and compare the initial state and every completed logical replacement to fresh Dijkstra;
5. run ADES non-timed exact mode and compare initialization and every logical replacement to fresh Dijkstra;
6. reject any mismatch, missing output, timeout, process failure, topology drift, source drift, or distance-bound violation.

Rejected disposition:

`REJECTED_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND`

No performance result is admitted after such a rejection.

## Timing contract

Identical primary timing semantics to PR70/PR72.

GraphBolt internal algorithm time:

- initialization = reported `Initial graph processing`
- maintenance = sum of all 256 reported `Finished batch` times
- total = initialization + maintenance

ADES internal algorithm time:

- initialization = one exact fixed-source SSSP construction
- maintenance = all 128 graph weight replacements plus resident repair/controller-equivalent work and exact-Dijkstra fallback where invoked
- total = initialization + maintenance

Input download, parsing, weight/workload generation, correctness serialization, fresh-oracle computation, build time, and process startup are excluded from primary internal algorithm time and disclosed separately where available.

## Repetitions and order balancing

For each scenario:

- repeat 0: GraphBolt -> ADES
- repeat 1: ADES -> GraphBolt

Total measured pairs: 8.

GraphBolt uses `OMP_NUM_THREADS=4` and pinned mimalloc.

## Statistical reporting and decision rule

For maintenance and total time:

1. compute GraphBolt/ADES ratio per measured pair;
2. geometric-mean the two repetitions within each scenario;
3. geometric-mean the four scenario-level ratios;
4. report deterministic 95% bootstrap intervals over the four scenario-level ratios with 20,000 resamples and RNG seed `74074`.

The interval is a **within-one-topology scenario-robustness interval**, not cross-topology population inference.

Preferred hypothesis support requires:

- 4/4 scenarios exact for both implementations;
- all topology/source/distance gates pass;
- all 8 measured pairs complete;
- maintenance aggregate GM > 1 and bootstrap lower bound > 1;
- total aggregate GM > 1 and bootstrap lower bound > 1;
- no scenario-level maintenance ratio < 1.

If a complete experiment misses those criteria:

`FALSIFIES_PREFERRED_PR74_GNUTELLA08_HYPOTHESIS_ON_TESTED_SLICE`

If all criteria hold:

`SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_ON_PR74_GNUTELLA08_SLICE`

CI must treat either completed result as success. It fails only for incomplete or semantically inadmissible experiments.

## Cross-topology interpretation

PR74 is intentionally the second external real topology after PR72. Even if both support ADES, the four scenarios per graph remain within-graph conditions rather than independent graph samples.

A successful PR74 would support a stronger **two-distinct-real-topology empirical robustness statement**, but not a statistically general all-topology claim and not universal/SOTA language.

## Claim boundary

A supporting PR74 permits only:

**On the exact frozen SNAP p2p-Gnutella08 directed topology, under the preregistered deterministic synthetic positive weights, four fixed SCC-source scenarios, and frozen dynamic weight-replacement workload, the ADES resident fixed-source core outperformed the PR69-qualified GraphBolt/DZiG comparator under the reported timing contract. Together with PR72, this constitutes supporting evidence on two distinct real directed topology domains.**

It does not establish native Gnutella weighted performance, arbitrary-source superiority, all GraphBolt configurations, cross-topology population inference, universal superiority, theoretical SOTA, or all-graphs practical SOTA.
