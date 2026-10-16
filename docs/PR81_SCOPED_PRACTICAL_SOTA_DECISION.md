# PR81 — Scoped practical-SOTA evidence synthesis

## Decision

**GO — SCOPED PRACTICAL SOTA, under the frozen Publication Contract v1.0 claim boundary.**

This decision does **not** assert universal dynamic-shortest-path SOTA, asymptotic novelty, or superiority over every published algorithm. It applies only to the frozen `C-SOTA` domain and the implementations that have passed the repository's qualification rules under matched assumptions.

The frozen headline claim is:

> **C-SOTA.** Within the qualified exact-online comparator set and the frozen benchmark scope, ADES establishes or materially improves the runtime-memory Pareto frontier under at least a defensible subset of mixed-update, repeated-source workloads.

PR81 classifies that scoped claim as **SUPPORTED** by the post-PR41 evidence chain described below.

## Claims addressed

```text
claim_ids: [C1, C2, C3, C4, C-SOTA]
```

PR81 is an evidence-synthesis and bookkeeping step. It does not change ADES performance logic, benchmark seeds, timing definitions, comparator source code, or the frozen Publication Contract.

## 1. What changed since the contract freeze

At PR41, `C-SOTA` was intentionally `UNESTABLISHED`. The later campaign added the missing evidence classes instead of relaxing the claim:

1. canonical matched traces and independent exact oracles;
2. auditable common persistent-state byte accounting;
3. pinned external exact recomputation evidence;
4. pinned external dynamic-maintenance evidence with arbitrary-source adaptation;
5. fresh source-reuse holdouts;
6. fresh non-road holdouts;
7. fixed-source historical-comparator stress tests on independent real topologies;
8. explicit preservation of losses/falsification paths and outcome-neutral CI gates.

The decisive new evidence is not a single favorable benchmark. It is the convergence of independent comparator and topology campaigns under predeclared constraints.

## 2. Direct comparator evidence

### 2.1 Boost.Graph Dijkstra — exact recomputation baseline

Boost.Graph 1.92.0 is already recorded as `DIRECT` in the comparator registry. The adapter executes fresh exact Dijkstra for arbitrary-source queries, consumes updates online, retains no shortest-path state across operations, and is governed by the common trace/oracle/timing model.

Post-qualification head-to-head evidence includes:

- PR61 matched NY comparisons against frozen ADES candidate `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`;
- PR62 fresh 24-cell holdout: exact in all cells, approximately `5.112681×` aggregate total advantage and `5.059393×` query advantage for ADES, with zero reported total/query loss cells.

Boost is therefore the strong external **cold exact recomputation** control. It is not a dynamic-maintenance algorithm, and PR81 does not present it as one.

### 2.2 VeloGraphX — qualified dynamic-maintenance comparator

Pinned upstream:

- repository: `sauravsingla/VeloGraphX`;
- commit: `2aa84ce81a180487239c661b65639e5100136075`;
- license: Apache-2.0;
- upstream algorithm modifications: none;
- ADES-side adapter blob: `b09a9f51b2b0300dce2801d0a8afaa49440d45a6`;
- matched pair driver blob: `44019d478377282b5056a6b07d86d76757f7efc0`.

The adapter preserves directed endpoint semantics, maps parallel ADES edges to the minimum endpoint weight required by VeloGraphX's simple-graph representation, and adds arbitrary-source behavior as a byte-bounded LRU of independent fixed-source VeloGraphX instances. Every retained graph + SSSP instance is charged to the logical-state budget. The upstream VeloGraphX algorithm remains unchanged.

PR64/PR65 established semantic qualification and the DIRECT adapter path. PR66 and PR80 then attacked source reuse and topology generalization without tuning the frozen ADES candidate.

#### PR66 — fresh NY-road source-reuse holdout

- candidate: `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`;
- 24 exact cells;
- two road graphs × four source regimes × three fresh workload seeds;
- 64 MiB logical-state budget;
- aggregate VeloGraphX/ADES total geometric mean: `15.561191×`;
- aggregate query-only geometric mean: `3.897866×`;
- total/query loss cells: `0 / 24`;
- high-reuse families exercised real VeloGraphX cache hits;
- canonical run: `38730045262`;
- job: `116405296228`;
- artifact: `11971060916`.

#### PR80 — fresh non-road VeloGraphX cross-product holdout

PR80 was preregistered specifically to falsify the possibility that the PR66 result was road-domain-specific.

Frozen design:

- three fresh deterministic preferential-attachment graphs;
- graph seeds `8209`, `9103`, `10037`;
- 15,000 vertices and 74,985 logical undirected edges per graph;
- identical symmetric directed representation for ADES and VeloGraphX;
- two fresh workload seeds `12007`, `13001`;
- source regimes: `uniform`, `hot-pool-2`, `hot-pool-4`, `rotating-hot-4`;
- 60 queries and 12 logical alternating weight replacements per cell;
- 24 total cells;
- two order-balanced matched repetitions per cell;
- 64 MiB logical persistent-state budget for both implementations;
- fresh ADES-Dijkstra = Boost.Graph-Dijkstra oracle agreement at every measured query;
- valid performance loss would have been accepted by CI.

The first PR80 execution exposed a harness-only directory-creation defect before the first oracle/timing comparison. The repair only moved output-directory creation before process invocation; no graph, trace, seed, comparator, budget, repetition, metric, or threshold changed.

Canonical repaired execution:

- head: `d7ff1eea51db6c07b312af393c315d6c84bcd661`;
- workflow run: `38856829047`;
- job: `116830072333`;
- artifact: `12027088809`;
- artifact digest: `sha256:617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100`;
- disposition: `SUPPORTS_ADES_OVER_VELOGRAPHX_ON_PR80_NONROAD_ARBITRARY_SOURCE_SLICE`.

PR80 results:

| Metric | VeloGraphX / ADES geometric mean | 95% CI | Loss cells |
|---|---:|---:|---:|
| total algorithm time | `157.677417×` | `[95.329154, 260.803405]` | `0 / 24` |
| query-only time | `30.199379×` | `[16.218261, 56.233063]` | `0 / 24` |
| update time | `90,903.427654×` | `[41,898.462269, 197,225.213331]` | diagnostic only |

The deliberately hard preregistered rule also required every source-family and every graph-instance lower 95% bound to remain above `1`, plus zero individual total/query loss cells. All conditions passed.

Worst observed PR80 cells still favored ADES:

- minimum total cell ratio: `30.153080×`;
- minimum query-only cell ratio: `6.797390×`.

Source-family lower bounds were also all strongly above one:

- `hot-pool-2`: total lower bound `37.161193×`, query lower bound `7.091532×`;
- `hot-pool-4`: total lower bound `66.030321×`, query lower bound `8.909928×`;
- `rotating-hot-4`: total lower bound `70.018159×`, query lower bound `11.620284×`;
- `uniform`: total lower bound `815.160136×`, query lower bound `269.736506×`.

VeloGraphX cache behavior confirms that the source-reuse cells actually exercised retained state rather than degenerating into cold recomputation:

- cache hits: `996`;
- cache misses: `444`;
- cache evictions: `204`.

Maximum accounted logical state remained inside the 64 MiB contract:

- VeloGraphX: `65,507,832` bytes;
- ADES: `1,802,576` bytes.

Process RSS was separately observed, not used as the fairness budget:

- VeloGraphX max RSS: `311,232 KiB`;
- ADES max RSS: `18,896 KiB`.

PR66 + PR80 therefore supply independent road and non-road evidence against the same pinned dynamic-maintenance comparator under arbitrary-source workloads and the same logical-state contract.

## 3. Contextual comparator evidence that does not broaden C-SOTA

### NetworKit 11.2.2

The later NetworKit campaign qualified a newer implementation for the tested undirected/symmetric workload scope and PR68 produced a fresh non-road 24-cell holdout with exactness and zero loss cells. Its aggregate NetworKit/ADES total geometric mean was approximately `6.230485×`, with query geometric mean approximately `6.092121×`.

However, the frozen primary `C-SOTA` domain is directed. The newer NetworKit evidence is therefore retained as **contextual cross-comparator/non-road evidence**, not silently promoted to unrestricted DIRECT evidence for the complete frozen domain.

### GraphBolt / DZiG historical Cilk path

PR76 qualified the historical Cilk build path for its fixed-source weighted streaming-SSSP scope. PR77, PR78, and PR79 then tested ADES's resident fixed-source core against that comparator.

Real-topology results include:

- PR78 SNAP `email-Eu-core`: 4/4 exact scenarios, GraphBolt/ADES maintenance GM `389.747960×`, total GM `31.455808×`;
- PR79 SNAP `p2p-Gnutella08`: 4/4 exact scenarios, GraphBolt/ADES maintenance GM approximately `63.73×`, total GM approximately `22.71×`.

These results strengthen the engineering case that ADES's fixed-source dynamic core is highly competitive on the tested slices. They remain **contextual** to `C-SOTA` because GraphBolt's tested historical path is fixed-source and has a bounded integer-distance domain rather than the complete arbitrary-source publication contract.

## 4. Frozen-claim decision table

| Claim | PR81 classification | Basis |
|---|---|---|
| C1 — exactness | **SUPPORTED** for the performance evidence cited here | every central matched query passed the declared independent oracle gates |
| C2 — trace parity | **SUPPORTED** | cryptographic graph/trace identities and matched pair drivers are enforced |
| C3 — fair persistent-state budget | **SUPPORTED** | common auditable byte budget is enforced; RSS remains separately reported |
| C4 — runtime-memory tradeoff | **SUPPORTED in defined regimes** | ADES wins against cold exact recomputation and qualified dynamic maintenance under equal declared logical-state controls |
| C-SOTA — scoped runtime-memory frontier | **SUPPORTED / GO — SCOPED SOTA** | Boost DIRECT baseline plus VeloGraphX DIRECT dynamic-maintenance evidence, with road and non-road holdouts, exactness, arbitrary sources, mixed updates, source reuse, and memory accounting |

PR81 does **not** reclassify C5–C9. Their own ablation/mechanism evidence remains governed by the existing campaign artifacts and should be reported independently in a paper.

## 5. Strongest defensible headline wording

The repository may now use wording equivalent to:

> **Under the frozen exact-online CPU benchmark contract—directed nonnegative weighted graphs, mixed online weight increases/decreases, exact arbitrary-source point-to-point queries, no future knowledge, and bounded auditable persistent algorithm state—ADES establishes a scoped practical runtime-memory frontier improvement over the qualified comparator set on the tested workloads. In fresh matched holdouts it outperformed both an exact Boost.Graph recomputation baseline and a pinned VeloGraphX dynamic-maintenance comparator, including independent road and non-road source-reuse workloads.**

Short form, when context is immediately visible:

> **Scoped practical SOTA evidence under the repository's frozen exact-online benchmark contract.**

## 6. Wording that remains prohibited

Do **not** claim any of the following from this evidence:

- “ADES is the world's fastest dynamic shortest-path algorithm.”
- “ADES beats all state-of-the-art shortest-path algorithms.”
- “ADES establishes theoretical SOTA.”
- “ADES asymptotically improves Dijkstra.”
- “ADES dominates every graph family or workload distribution.”
- “ADES beats VeloGraphX natively at arbitrary-source semantics.” The arbitrary-source comparison uses the transparent byte-bounded adapter documented in the repository.
- “Equal RSS.” The fairness contract is equal/auditable algorithm-owned logical state; RSS is separately disclosed and differs materially.

## 7. Remaining publication-strengthening work

The scoped practical-SOTA decision is now supported, but publication strength can still improve through **orthogonal evidence**, not favorable-cell accumulation:

1. register the post-PR48 VeloGraphX qualification explicitly in the machine-readable comparator registry;
2. preserve a machine-readable PR81 evidence manifest linking all canonical runs/artifacts;
3. add at least one further independent real directed arbitrary-source topology if a clean DIRECT comparator adapter supports it without semantic distortion;
4. run an adversarial paper-level audit of claim wording, artifact reproducibility, statistical independence, and comparator assumption matching;
5. keep pending literature-only algorithms and mismatched preprocessing/approximation classes segregated rather than presenting absence of an executable match as a win.

## 8. R7 verdict

**GO — SCOPED PRACTICAL SOTA.**

The dominant uncertainty is no longer whether ADES can beat a credible exact-online comparator on the tested CPU slices. It can, and the evidence survives source reuse, memory accounting, independent oracles, a dynamic-maintenance comparator, and road/non-road topology changes.

The remaining risk is **claim breadth**, not demonstrated slice performance. The correct next move is therefore evidence registration, adversarial publication audit, and carefully bounded manuscript wording—not another unconstrained optimization rewrite.