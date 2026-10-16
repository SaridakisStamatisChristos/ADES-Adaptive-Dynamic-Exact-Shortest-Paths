# PR72 — GraphBolt vs ADES on frozen SNAP email-Eu-core topology

## Status

**Preregistered performance contract.** Comparative timing is forbidden until the complete contract, harness, and CI workflow are committed together on this branch.

PR72 is the first ADES-vs-GraphBolt performance gate on an externally sourced real topology. It is intentionally narrow and does not claim that synthetic weights are native measurements or that one topology establishes general SOTA.

## Lineage and frozen evidence

Base branch/head:

- PR71 branch: `pr71-email-eu-core-topology-freeze`
- canonical PR71 head: `0220153da9ab087d4ed6dc5d999554a3d5c662bf`

Frozen external topology identity from PR71:

- source: SNAP `email-Eu-core`
- URL: `https://snap.stanford.edu/data/email-Eu-core.txt.gz`
- raw gzip SHA-256: `4b47acdb80197b085fe63c819c357ae488131ee904ed93d1b219a68b0f9e245f`
- canonical full directed edge-list SHA-256: `f0cfcb0a49f8d12fa6a92061a93d07becd8808185494e9d73d4f6695809e9d5e`
- vertices: 1,005, IDs exactly `0..1004`
- directed edges: 25,571 unique arcs
- self-loops: 642, preserved from source
- largest SCC: 803 nodes / 24,729 directed internal edges
- PR71 canonical run: `38841945651`
- PR71 canonical artifact: `12021033601`
- PR71 artifact digest: `sha256:05702c8f939e3444e2ddcca267074454f306885ce1ec76dad54f172ac98531a8`

GraphBolt semantic lineage:

- GraphBolt/DZiG commit: `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- mimalloc v1.6.0 commit: `04f1c3b1e23677ce03bd16137e73089abd552175`
- PR69-qualified portability repair: `kickstarter-missing-return-v1`
- qualified semantic scope: directed fixed-source positive-integer weighted streaming SSSP with finite distances `<65535`
- PR69 accepted both OMP1 and OMP4 after 9/9 exact cases / 729 checks

ADES execution lineage:

- reuse the unchanged PR70 fixed-source runner `tools/pr70_ades_fixed_source.cpp`
- resident fixed-source state only; no arbitrary-source cache admission/eviction policy
- the runner's exact-Dijkstra fallback remains charged when used

Any additional GraphBolt source modification, ADES runner modification, or topology identity change invalidates this gate and requires a new semantic/preperformance qualification.

## External topology and synthetic weights

The topology is real and externally sourced; the edge weights are **synthetic deterministic positive integers**. No native email latency, distance, cost, or duration is claimed.

For each scenario, every source edge `(u,v)` including self-loops receives a deterministic integer weight in `[1,30]` from a fixed hash of `(weight_seed,u,v)`.

Positive self-loops are retained in the initial graph for source fidelity. They are never selected for dynamic weight replacement because changing a positive self-loop cannot alter a nonnegative SSSP solution; timing such changes would measure semantically inert update overhead rather than meaningful dynamic shortest-path maintenance.

No other topology edge is removed or added by the harness.

## Frozen source selection

Sources are selected without performance information from fixed quantiles of the **sorted node IDs in PR71's 803-node largest SCC**.

Quantile positions and sources:

- 12.5% position 100 -> source `102`
- 37.5% position 301 -> source `308`
- 62.5% position 501 -> source `525`
- 87.5% position 702 -> source `828`

The harness must independently recompute the largest SCC from the frozen topology and hard-fail unless its sorted quantile selection equals exactly `[102, 308, 525, 828]`.

## Frozen scenarios

Four robustness scenarios are used on this **single topology**:

| scenario | source | weight seed |
|---|---:|---:|
| S0 | 102 | 7201 |
| S1 | 308 | 7207 |
| S2 | 525 | 7211 |
| S3 | 828 | 7213 |

These scenarios are not independent topology replications. Any bootstrap interval reported over them is a within-topology scenario-robustness interval only and must not be described as population-level external-topology inference.

## Dynamic workload

For each scenario:

- 128 logical weight replacements;
- candidate update edges are all non-self-loop edges only;
- exact topology endpoints never change;
- replacements alternate decrease/increase where feasible;
- decrease delta is an integer in `[1,min(7,old-1)]` and only edges with `old>1` are eligible;
- increase delta is an integer in `[1,7]`;
- update edge selection is deterministic from the scenario weight seed XOR fixed constant `0x72535550`;
- repeated selection of an edge is allowed and its current weight is used;
- GraphBolt represents each logical replacement as `delete(u,v,old)` followed by `add(u,v,new)` and is charged for both streaming batches;
- ADES is charged for the corresponding logical weight replacement and all repair/controller/fallback work.

No update seed, edge set, delta, source, scenario, or count may change after comparative outcomes are observed.

## GraphBolt distance bound

Before either comparator is admitted to timing, the harness must replay every logical state with fresh Dijkstra and hard-fail if any finite oracle distance is `>=65535`.

Unreachable vertices are represented by GraphBolt's application sentinel `65535` and are not treated as finite-bound violations.

## Correctness-before-performance gate

For every scenario, before any measured pair:

1. GraphBolt runs a separate non-timed correctness pass with full vertex output;
2. its initial state and every **completed logical replacement** are compared with fresh Dijkstra;
3. ADES runs a separate exact pass and is checked against fresh Dijkstra after initialization and every logical replacement;
4. all finite-distance bounds must pass;
5. the topology hashes, source set, edge count, self-loop count, and workload manifests must match their frozen rules.

Any mismatch, timeout, process failure, missing output, topology/hash drift, or bound violation yields:

`REJECTED_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND`

and no comparative timing is admitted.

PR69 already qualified GraphBolt's intermediate delete and add phases semantically; PR72's equality gate is evaluated after each completed logical replacement so it compares the same logical graph state as ADES.

## Timing contract

The timing boundary is inherited from PR70.

GraphBolt internal algorithm time:

- initialization = reported `Initial graph processing`
- maintenance = sum of all 256 reported `Finished batch` times
- total = initialization + maintenance

ADES internal algorithm time:

- initialization = initial fixed-source exact SSSP construction
- maintenance = all 128 graph weight replacements plus resident repair/controller-equivalent work and exact-Dijkstra fallback where invoked
- total = initialization + maintenance

Input download, topology parsing, weight generation, workload generation, correctness serialization, fresh oracle computation, build time, and process startup are excluded from the primary internal algorithm metric and disclosed separately through provenance/process wall time.

## Execution order and repetitions

Each scenario receives two measured pairs:

- repeat 0: GraphBolt -> ADES
- repeat 1: ADES -> GraphBolt

Thus the frozen experiment has 8 measured pairs total.

GraphBolt primary runtime is `OMP_NUM_THREADS=4`, matching PR70 and the PR69-qualified OMP4 configuration. `LD_PRELOAD` must point at the pinned mimalloc v1.6.0 library.

## Statistical reporting and decision rule

For maintenance and total time:

1. compute GraphBolt/ADES ratio for every measured pair;
2. compute the geometric mean of the two order-balanced repetitions within each scenario;
3. compute the geometric mean across the four frozen scenario-level ratios;
4. report a deterministic 95% bootstrap interval over the four scenario-level ratios using 20,000 resamples and RNG seed `72072`.

The bootstrap is explicitly a **within-topology scenario-robustness summary**, not an independent-topology confidence interval.

The narrow preferred hypothesis is supported only if:

- all 4 scenarios pass exact correctness and topology/distance gates;
- all 8 measured pairs complete;
- maintenance aggregate GM > 1 and bootstrap lower bound > 1;
- total aggregate GM > 1 and bootstrap lower bound > 1;
- no scenario-level maintenance ratio < 1.

If the experiment completes but those conditions are not met, the accepted disposition is:

`FALSIFIES_PREFERRED_PR72_EXTERNAL_TOPOLOGY_HYPOTHESIS_ON_TESTED_SLICE`

If they are met:

`SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_ON_PR72_EMAIL_EU_CORE_SLICE`

CI must treat either completed disposition as a successful experiment. It must fail only when the semantic/topology/distance gate is rejected or the experiment fails to complete.

## Claim boundary

A supporting PR72 permits only:

**On the exact frozen SNAP email-Eu-core directed topology, under the preregistered deterministic positive synthetic weights, four fixed SCC-source scenarios, and frozen dynamic weight-replacement workload, the ADES resident fixed-source core outperformed the PR69-qualified GraphBolt/DZiG comparator under the reported timing contract.**

It does not establish:

- native email-weight performance;
- arbitrary-source superiority;
- all GraphBolt configurations or historical Cilk superiority;
- equal-memory superiority unless separately measured;
- cross-topology statistical generalization;
- universal superiority;
- theoretical SOTA;
- all-graphs practical SOTA.

A second external topology remains required before stronger external-generalization language is considered.
