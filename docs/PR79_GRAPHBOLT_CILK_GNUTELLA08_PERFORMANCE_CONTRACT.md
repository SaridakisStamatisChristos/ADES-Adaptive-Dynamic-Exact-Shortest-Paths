# PR79 — preregistered historical-Cilk GraphBolt vs ADES on SNAP p2p-Gnutella08

## Status

**PREREGISTERED BEFORE PR79 COMPARATIVE TIMING.**

PR79 is the second external-topology replication of the PR77/PR78 historical-Cilk campaign. It changes only the GraphBolt execution substrate relative to the already-qualified PR74 `p2p-Gnutella08` experiment: from the PR69-qualified OpenMP4 path to the PR76-qualified historical Cilk Plus path. The topology, source scenarios, deterministic weights, logical updates, ADES runner, correctness gates, timing semantics, repetition structure, and statistical decision rule remain inherited from PR74.

A completed loss is accepted evidence. CI must not encode a preferred winner.

## 1. Frozen evidence lineage

### PR79 base

PR79 base before these preregistration files:

`97a1cbc2dfdea4833432fea173f08e7c3d433979`

This is the canonical PR78 head.

ADES fixed-source runner remains:

`tools/pr70_ades_fixed_source.cpp`

The runner remains unchanged. It maintains one resident exact `SSSPState` with the repository's decrease/increase repair logic and exact-Dijkstra fallback. Arbitrary-source cache admission, eviction, and cold-query policy are outside this comparator contract.

### PR78 real-topology historical-Cilk prerequisite

Canonical PR78 execution on SNAP `email-Eu-core`:

- head: `97a1cbc2dfdea4833432fea173f08e7c3d433979`
- workflow run: `38855443465`
- job: `116825732902`
- artifact: `12027485073`
- artifact digest: `sha256:58e5c09142282a0baf0ec3853ea9993c8e3b3937348bb92679dc065050316d3d`
- exact scenarios: 4/4
- measured pairs: 8/8
- maintenance GraphBolt/ADES geometric mean: `389.7479595838792`
- total GraphBolt/ADES geometric mean: `31.455808022699205`
- disposition: `SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR78_EMAIL_EU_CORE_SLICE`

PR78 demonstrates that the historical-Cilk execution path survives one frozen real external topology. It is context only for PR79; PR79 must independently repeat correctness and timing on Gnutella08.

### PR76 historical-Cilk semantic qualification

- accepted head: `e5e06aef3b6d35aca40c98183553755dd134f3fe`
- workflow run: `38854118754`
- job: `116821640368`
- artifact: `12026410989`
- artifact digest: `sha256:56fd8c8cadc3ed41414f3b1bdbefe62a8aebd15f81c261ef2742720b0acc0166`
- result: 9/9 exact cases, 729/729 checks

Qualification:

`QUALIFIED_HISTORICAL_CILK_DIRECTED_FIXED_SOURCE_WEIGHTED_STREAMING_SSSP_WITH_UINT16_DISTANCE_BOUND`

### PR74 frozen Gnutella08 experiment lineage

Canonical OpenMP4 PR74 execution:

- head: `e470143b5d2410cea1ff840aedb5436156f99a96`
- workflow run: `38843018312`
- job: `116787170408`
- artifact: `12022600551`
- artifact ZIP SHA-256: `6c5bdfa201bb6a9068747cf9001b22ed38a7167f3ad858c5e09a4296795db06c`
- exact scenarios: 4/4
- measured pairs: 8/8
- maintenance GraphBolt/ADES geometric mean: `36.6409386377`
- total GraphBolt/ADES geometric mean: `10.3754309691`
- disposition: `SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_ON_PR74_GNUTELLA08_SLICE`

PR74 freezes the experiment semantics. PR79 does not retune them.

## 2. Frozen comparator identity

GraphBolt / DZiG:

- repository: `pdclab/graphbolt`
- commit: `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- application: weighted `apps/SSSP.C`
- mimalloc commit: `04f1c3b1e23677ce03bd16137e73089abd552175` (v1.6.0)
- immutable historical Cilk image: `gcc@sha256:1938a2199146f37d7c5b697a66d4379b75e3cd6ca5e08380c6803ab25e0ba9a4`
- GraphBolt build: `-std=c++14 -fcilkplus -O3 -DCILK -DEDGELONG -DEDGEDATA -include cstdint`
- worker setting: `CILK_NWORKERS=4`
- allocator build mode: GNU C11 (`-std=gnu11`)
- only source repair: PR69-qualified `kickstarter-missing-return-v1`, one missing `return 0;` whose caller ignores the return value

Any other GraphBolt source change invalidates PR79 and requires fresh semantic qualification.

## 3. Frozen external topology identity

PR79 reuses the exact PR73/PR74 SNAP `p2p-Gnutella08` source artifact and topology semantics.

Hard-pinned identity:

- source: SNAP `p2p-Gnutella08`
- raw gzip URL: `https://snap.stanford.edu/data/p2p-Gnutella08.txt.gz`
- raw gzip SHA-256: `cb3019062be21e55b03f19a73e88edf34693bafb68ae581621cc7a037fa604a9`
- canonical sorted directed edge-list SHA-256: `4554021f32967c69b68ccf59f9d85b83b66a6c0413b721112a10741858fdf845`
- vertices: 6,301 with IDs `0..6300`
- unique directed edges: 20,777
- duplicate rows: 0
- self-loops: 0
- largest SCC: 2,068 nodes

No graph transformation is permitted.

## 4. Frozen weighted scenarios

The external topology is unweighted. PR79 therefore inherits PR74's deterministic **synthetic positive** weight semantics and makes no native Gnutella latency/cost/distance claim.

Four frozen scenarios:

| scenario | source | SCC position | weight seed | frozen max finite oracle distance |
|---|---:|---:|---:|---:|
| S0 | 617 | 258 | 7401 | 217 |
| S1 | 2079 | 775 | 7407 | 208 |
| S2 | 3676 | 1292 | 7411 | 212 |
| S3 | 5306 | 1809 | 7417 | 247 |

For each scenario:

- edge weights are deterministic integers in `[1,30]` under the unchanged PR74 function;
- 128 logical weight replacements are generated by the unchanged PR74 harness;
- replacements alternate decrease/increase under the frozen PR74 rules;
- GraphBolt implements one logical replacement as `delete(old)` then `add(new)` and is charged both batches;
- ADES performs one native logical weight replacement and all repair/controller/fallback work;
- repeated edge selection remains allowed using current weight state.

No topology, source, weight seed, update rule, update count, thread/worker count, timeout, or decision threshold may change after comparative timing outcomes are observed.

## 5. Correctness-before-performance gate

Performance is inadmissible unless every frozen scenario passes all gates.

For each scenario, before any measured pair:

1. fresh-download the exact SNAP artifact and verify its frozen raw SHA-256;
2. reconstruct and verify the frozen canonical topology SHA-256, node/edge counts, zero duplicates, zero self-loops, SCC size, source IDs, and frozen source positions;
3. replay every logical state with fresh Dijkstra and require the frozen per-scenario maximum finite distances `[217,208,212,247]`, each `<65535`;
4. run historical-Cilk GraphBolt non-timed with full output and compare initialization plus every completed logical replacement against fresh Dijkstra;
5. run ADES non-timed exact mode on the identical graph/source/update stream and compare every logical state against fresh Dijkstra;
6. reject any mismatch, timeout, missing output, nonzero process exit, topology drift, source drift, or distance-bound violation.

Rejected disposition:

`REJECTED_PR79_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND`

No timing claim is admitted after this disposition.

## 6. Timing contract

Primary metrics remain the internal algorithm times inherited from PR74/PR77/PR78.

Historical-Cilk GraphBolt:

- initialization = reported `Initial graph processing`;
- maintenance = sum of all 256 reported `Finished batch` times;
- total = initialization + maintenance.

ADES:

- initialization = initial exact fixed-source Dijkstra;
- maintenance = all 128 graph weight replacements plus resident repair/controller/fallback work;
- total = initialization + maintenance.

Input download, topology parsing, deterministic weight/workload generation, correctness serialization, fresh-oracle computation, build time, Docker/process startup, and artifact packaging are excluded from the primary internal-algorithm metric. Process wall time remains a secondary disclosure.

## 7. Repetitions and execution order

For every scenario:

- repetition 0: GraphBolt -> ADES
- repetition 1: ADES -> GraphBolt

Total measured pairs: 8.

GraphBolt uses exactly four historical Cilk workers. ADES remains the frozen repository implementation.

## 8. Statistical rule

For both maintenance and total time:

1. compute GraphBolt/ADES ratio for every measured pair;
2. geometric-mean the two order-balanced repetitions within each scenario;
3. geometric-mean the four scenario-level ratios;
4. compute the deterministic PR74 bootstrap interval over scenario-level ratios using 20,000 resamples and RNG seed `74074`.

The interval is a **within-one-topology scenario-robustness interval**, not population-level cross-topology inference.

The preferred narrow hypothesis is supported only if:

- 4/4 scenarios are exact for both implementations;
- all topology/source/distance gates pass;
- all 8 measured pairs complete;
- maintenance aggregate geometric mean > 1 and bootstrap 95% lower bound > 1;
- total aggregate geometric mean > 1 and bootstrap 95% lower bound > 1;
- no scenario-level maintenance ratio is < 1.

Support disposition:

`SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR79_GNUTELLA08_SLICE`

Completed falsification/mixed disposition:

`FALSIFIES_PREFERRED_PR79_CILK_GNUTELLA08_HYPOTHESIS_ON_TESTED_SLICE`

CI must accept either completed performance disposition. Green means the preregistered experiment completed validly, not that ADES won.

## 9. Claim boundary

Even a supporting result permits only:

**On the exact frozen SNAP p2p-Gnutella08 directed topology, under the preregistered deterministic synthetic positive weights, four fixed SCC-source scenarios, and frozen dynamic weight-replacement workload, the ADES resident fixed-source core outperformed the PR76-qualified historical-Cilk GraphBolt/DZiG comparator under the reported timing contract. Together with PR78, this supplies historical-Cilk supporting evidence on two distinct real directed topology domains.**

It does not establish:

- native Gnutella weighted performance;
- native email weighted performance;
- arbitrary-source superiority;
- equal-memory superiority unless separately measured;
- population-level cross-topology inference;
- universal superiority;
- theoretical SOTA;
- all-graphs practical SOTA.

If PR79 supports the preferred hypothesis, the next highest-value R7 work is not another favorable fixed-source topology. It is to attack the remaining publication-contract gap: arbitrary-source/direct-comparator and memory-equivalence qualification under the frozen C-SOTA rules.