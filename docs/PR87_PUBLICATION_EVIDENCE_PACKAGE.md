# PR87 — Submission-grade publication evidence package

## Status

**PUBLICATION PACKAGE — evidence synthesis only.**

PR87 converts the completed PR41→PR86 scientific campaign into a submission-oriented evidence package without changing ADES, comparators, benchmark workloads, statistics, or the frozen Publication Contract v1.0.

The package preserves both favorable and unfavorable evidence. It is designed so that a manuscript can be written from canonical, versioned claims rather than reconstructed from memory or selectively favorable PRs.

## Final evidence-driven decision

### Performance

**GO — SCOPED PRACTICAL SOTA.**

Within the frozen exact-online CPU benchmark contract and qualified comparator set, the frozen ADES candidate materially improves the measured runtime-memory frontier on the tested workloads.

This is a scoped empirical claim. It is **not** universal dynamic-shortest-path SOTA and not theoretical/asymptotic SOTA.

### Novelty

**NO-GO for broad algorithmic/mechanism novelty.**

The focused PR86 prior-art audit found direct or conceptual prior-art collisions for dynamic SSSP repair, non-predecessor increase filtering, alternate-witness repair, generic cost-aware caching, maintenance-aware retention, and incremental-versus-full-recompute cost control.

The full ADES integration is **possibly distinct but novelty remains unestablished**. `NO_CLOSE_MATCH_FOUND` is not treated as proof of novelty.

## Frozen scientific scope

Publication Contract v1.0 restricts the primary `C-SOTA` domain to:

- directed weighted graphs;
- nonnegative edge weights;
- online operations;
- edge-weight increases and decreases;
- exact point-to-point shortest-path answers;
- no future-operation knowledge;
- bounded algorithm-owned persistent memory;
- repeated arbitrary query sources;
- adaptive source residency.

The strongest defensible headline remains:

> **Within the qualified exact-online comparator set and the frozen benchmark scope, ADES establishes or materially improves the runtime-memory Pareto frontier under a defensible subset of mixed-update, repeated-source workloads.**

## Frozen ADES candidate

Publication-facing candidate:

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

The formal algorithm description has been reconciled against this exact source in PR85.

Important frozen mechanisms include:

- exact cold bidirectional shortest-path search;
- bounded resident exact SSSP state per admitted source;
- exact online decrease repair;
- O(1) filtering of increased edges that are not the selected parent witness;
- selected-parent affected-subtree discovery and exact boundary-seeded repair;
- gated PR57-v2 alternate-witness rescue for already-large repairs;
- exact full-rebuild fallback;
- work-aware source admission;
- bounded causal eviction using reuse/recency/update burden;
- adaptive repair/rebuild budgeting;
- auditable persistent-state accounting.

These mechanisms are **not** collectively or individually declared novel by PR87.

## Qualified DIRECT comparator set

The canonical PR82 registry contains two DIRECT external implementations under the frozen 16-gate comparator contract.

### 1. Boost.Graph Dijkstra recomputation

Role: pinned exact arbitrary-source cold-recomputation baseline.

- Boost.Graph 1.92.0 / pinned implementation commit `fc5dac2c552f945846449fd123a9b8335d5c063a`;
- exact arbitrary-source queries by fresh Dijkstra;
- online weight mutation;
- zero retained shortest-path state;
- independent oracle and canonical trace qualification;
- DIRECT under the frozen registry.

### 2. VeloGraphX weighted SSSP through qualified adapter

Role: pinned dynamic-maintenance comparator.

- upstream repository: `sauravsingla/VeloGraphX`;
- upstream commit: `2aa84ce81a180487239c661b65639e5100136075`;
- upstream algorithm modification: none;
- adapter blob: `b09a9f51b2b0300dce2801d0a8afaa49440d45a6`;
- matched pair-driver blob: `44019d478377282b5056a6b07d86d76757f7efc0`;
- license: Apache-2.0.

Upstream VeloGraphX is fixed-source. Arbitrary-source behavior is therefore **adapter-mediated** through a causal byte-bounded LRU of independent fixed-source graph+SSSP instances. All retained state, source construction, updates, admission/eviction, and timing are charged/disclosed by the matched contract.

Do not describe VeloGraphX as natively arbitrary-source.

## Canonical performance evidence

### E1 — Boost.Graph fresh NY holdout, PR62

Frozen ADES candidate: `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`.

Canonical execution:

- PR #62;
- harness head: `9c851af7d235c8d508ef72feda3e39dcd5e452f4`;
- run: `38711826306`;
- artifact: `11961213712`;
- artifact SHA-256: `0bd64d43ba48fda4c9e0752ea6acafc1c9fa8ddb7eec2f2fdabc88d1574029ff`.

Result across 24 executed NY cells:

- exactness: 24/24;
- Boost/ADES total geometric mean: `5.112681×`;
- total 95% cell-level log-ratio interval: `[4.851231, 5.388222]`;
- query-only geometric mean: `5.059393×`;
- query 95% interval: `[4.800065, 5.332731]`;
- total losses: `0/24`;
- query losses: `0/24`;
- minimum total cell ratio: `4.073915×`;
- minimum query ratio: `4.032328×`.

#### Mandatory PR62 correction

PR62 originally labeled non-uniform source families, but a later audit found `locality_percent=0` forced the workload generator onto uniform-like source selection before the intended hot/churn branches.

Therefore PR62 **does support** external exact recomputation-baseline superiority on the executed NY trace slice, but **does not establish source-policy breadth** across hot-pool/rotating/churn regimes.

The numerical execution evidence remains valid. The invalid interpretation remains permanently corrected rather than deleted.

### E2 — VeloGraphX fresh NY source-reuse holdout, PR66

Canonical execution:

- PR #66;
- harness head: `26a38d300a475080bf9695d12247c8cb277946ee`;
- run: `38730045262`;
- job: `116405296228`;
- artifact: `11971060916`;
- artifact SHA-256: `c4a3d64af948afd03daa1e983ee2d75eadf838ca94bdc59103c25ff5575dd299`.

Frozen design:

- two NY road graphs;
- source regimes: uniform, hot-pool-2, hot-pool-4, rotating-hot-4;
- three fresh seeds `701,809,907`;
- 24 cells;
- 100 queries/cell;
- strict alternating increases/decreases;
- two order-balanced matched repetitions/cell;
- independent ADES-Dijkstra = Boost.Graph-Dijkstra oracle gate;
- 64 MiB common logical persistent-state cap;
- hot/rotating cells required ≥80% source reuse.

Result:

- exactness: 24/24;
- strong preregistered pass: true;
- VeloGraphX/ADES total GM: `15.561191×`;
- total 95% CI: `[14.569887, 16.619942]`;
- query-only GM: `3.897866×`;
- query 95% CI: `[2.982019, 5.094991]`;
- total losses: `0/24`;
- query losses: `0/24`;
- minimum total cell ratio: `11.298778×`;
- minimum query ratio: `2.290465×`.

Reuse was genuinely exercised: hot-pool cells produced `1,164` VeloGraphX cache hits and rotating-hot cells another `576` hits.

PR66 is the experiment that re-earned the source-reuse breadth missing from PR62.

### E3 — VeloGraphX fresh non-road holdout, PR80

Canonical repaired execution:

- PR #80;
- head: `d7ff1eea51db6c07b312af393c315d6c84bcd661`;
- run: `38856829047`;
- job: `116830072333`;
- artifact: `12027088809`;
- artifact digest: `sha256:617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100`.

Frozen design:

- three fresh deterministic preferential-attachment graph instances;
- graph seeds `8209,9103,10037`;
- 15,000 vertices and 74,985 logical undirected edges/graph, represented identically as symmetric directed arcs;
- workload seeds `12007,13001`;
- source regimes: uniform, hot-pool-2, hot-pool-4, rotating-hot-4;
- 60 queries and 12 logical alternating weight replacements/cell;
- 24 cells;
- two order-balanced repetitions/cell;
- 64 MiB common logical persistent-state cap;
- independent ADES-Dijkstra = Boost.Graph-Dijkstra oracle agreement at every measured query;
- valid performance loss accepted by the workflow.

Result:

- exactness: 24/24;
- total VeloGraphX/ADES GM: `157.677417×`;
- total 95% **slice-level robustness** interval: `[95.329154, 260.803405]`;
- query-only GM: `30.199379×`;
- query 95% slice-level interval: `[16.218261, 56.233063]`;
- total losses: `0/24`;
- query losses: `0/24`;
- minimum total cell ratio: `30.153080×`;
- minimum query cell ratio: `6.797390×`;
- every source-family total/query lower bound >1;
- every graph-instance total/query lower bound >1;
- VeloGraphX cache hits/misses/evictions: `996 / 444 / 204`.

Accounted logical-state peaks:

- VeloGraphX: `65,507,832` bytes;
- ADES: `1,802,576` bytes;
- common cap: `67,108,864` bytes.

These are **not equal actual bytes**. The fairness rule is a common maximum auditable algorithm-owned persistent-state allowance. Process RSS is reported separately and is not substituted for that contract.

The PR80 intervals characterize robustness across this preregistered nested slice; they are **not population-level inference over all graphs/workloads**.

## Contextual performance evidence

### Historical-Cilk GraphBolt / DZiG

PR76 semantically qualified the historical Cilk path for a fixed-source weighted streaming-SSSP scope. PR78 and PR79 then supplied real-topology contextual evidence:

- PR78 SNAP `email-Eu-core`: 4/4 exact; GraphBolt/ADES maintenance GM `389.747960×`; total GM `31.455808×`;
- PR79 SNAP `p2p-Gnutella08`: 4/4 exact; maintenance GM approximately `63.73×`; total GM approximately `22.71×`.

These are strong fixed-source engineering results, but they remain contextual for the complete arbitrary-source `C-SOTA` domain.

### NetworKit

Later NetworKit evidence includes a fresh non-road holdout favorable to ADES, but the qualified scope is not equivalent to the full directed arbitrary-source publication contract. NetworKit remains contextual rather than silently promoted to unrestricted DIRECT evidence.

## Negative, rejected and corrective evidence that must remain visible

### N-E1 — PR57 v1 eager witness probe rejected

PR57 v1 was **not accepted**. A 12-cell matrix found a statistically supported regression on clustered hot-pool / `FREQ-LRU-REPAIR`:

`PR55 / PR57-v1 = 0.910073×`, 95% CI `[0.872704, 0.949041]`.

PR57-v2 was redesigned before acceptance:

- zero witness scans below 256 affected vertices;
- one bounded scan of at most 64 incoming root arcs at the trigger;
- exact strict/equal-distance safety checks;
- no extra persistent shortest-path DAG.

The accepted broad v2 regression matrix found 0 statistically supported improvements, 0 regressions, 12 inconclusive; the targeted mechanism cases retained very large causal wins. This is an engineering mechanism result, not an ordinary-workload SOTA claim.

### N-E2 — affected-subtree size is not a sufficient rebuild policy

The adaptive repair controller's broad economic hypothesis

> large affected subtree ⇒ rebuilding is economically preferable

is partially refuted by archived NY catastrophic-cut evidence. The controller can correctly identify structurally large damage while abort+rebuild is still slower than completing repair.

A better future policy should estimate **remaining repair cost versus rebuild cost** rather than use affected-region size as a sufficient economic proxy.

### N-E3 — PR62 source-family interpretation correction

Already described under E1. Keep the numerical execution evidence; reject the invalid source-policy breadth interpretation.

### N-E4 — PR80 first-run harness defect

The first PR80 execution failed before comparative timing because the oracle output directory did not exist. The repair only created the directory before process invocation. No graph, seed, trace, comparator, budget, repetitions or decision threshold changed. The canonical repaired run is the only admitted performance result.

### N-E5 — adversarial audit harness defects

PR83/84 exposed several brittle prose/Markdown predicates. These were repaired as harness semantics, not treated as scientific refutations. The final adversarial audit has zero invalidating findings, H1 README drift closed by PR84, and G1 novelty retained open until PR86.

## Claim ledger for manuscript use

| Claim | Submission status | Allowed interpretation |
|---|---|---|
| C1 exactness | **SUPPORTED for central performance cells** | every admitted matched query passes the declared independent exact-oracle gates |
| C2 trace parity | **SUPPORTED** | matched algorithms consume identical cryptographically identified operation streams |
| C3 persistent-state fairness | **SUPPORTED** | bounded algorithms obey the same declared cap/accounting model; actual bytes/RSS need not be equal |
| C4 runtime-memory tradeoff | **SUPPORTED IN TESTED REGIMES** | ADES beats cold recomputation and adapter-qualified dynamic maintenance on the admitted slices |
| C5–C9 | **do not elevate from PR87** | use only their specific ablation/telemetry evidence and preserve contradictions/inconclusive cells |
| C-SOTA | **SUPPORTED WITHIN FROZEN SCOPE** | scoped practical runtime-memory frontier evidence, not universal/theoretical SOTA |
| broad mechanism novelty | **REJECTED** | multiple direct/conceptual prior-art collisions |
| integrated systems novelty | **UNESTABLISHED / POSSIBLY DISTINCT** | focused audit found no exact full-system match; this is not proof of novelty |

## Prior-art positioning from PR86

The manuscript must acknowledge at minimum:

- Ramalingam–Reps and Frigioni et al. for dynamic shortest-path repair;
- predecessor/non-predecessor update logic in pre-ADES dynamic-query work;
- replacement/alternate shortest-path witness techniques such as King–Thorup and Ramalingam–Reps descendants;
- shortest-path caching and dynamic cache-refresh literature;
- TinyLFU/cost-aware caching for admission/retention principles;
- materialized-view / memory-bounded incremental-query selection under update cost;
- dynamic-graph cost models that choose incremental processing versus full recomputation.

Do not use performance novelty as a substitute for literature novelty.

## Strongest submission title

> **ADES: Exact Adaptive Source Residency for Online Dynamic Shortest Paths Under Bounded State**

This avoids a novelty or universal-SOTA claim in the title.

## Manuscript-ready core contribution statement

> ADES combines exact online dynamic shortest-path maintenance with bounded adaptive residency of source-specific SSSP state. Under a frozen benchmark contract covering exact arbitrary-source queries, mixed online weight increases/decreases, no future knowledge and auditable persistent-state limits, the frozen implementation materially improves the measured runtime-memory frontier over two qualified external comparators on the tested CPU workloads. The constituent repair and caching ideas have substantial prior art; the paper therefore positions its contribution as an exact systems architecture, reproducible fairness methodology, operating-regime analysis and empirical runtime-memory result rather than as a new worst-case dynamic shortest-path bound.

## Submission-ready abstract

> Online shortest-path services face a tension between recomputing exact routes on demand and retaining source-specific state that accelerates repeated queries but incurs memory and update cost. We present ADES, an exact CPU architecture that adaptively moves query sources between cold point-to-point search and bounded resident single-source shortest-path state while processing online edge-weight increases and decreases without future knowledge. ADES uses exact local repair when valid and exact recomputation as a conservative fallback, and exposes auditable algorithm-owned persistent-state accounting for matched evaluation. We freeze the research question, comparator eligibility, timing boundaries and memory contract before the definitive experiments. Against a pinned Boost.Graph exact-recomputation baseline, ADES is 5.11× faster in total algorithm time on a 24-cell NY holdout, with no loss cells. Against a pinned VeloGraphX dynamic-maintenance implementation through a qualified byte-bounded arbitrary-source adapter, ADES is 15.56× faster on a fresh 24-cell NY source-reuse holdout and 157.68× faster on a preregistered 24-cell non-road holdout; all admitted cells pass independent exact-oracle checks and the latter holdout has no total- or query-time losses. These results support a scoped practical runtime-memory frontier improvement under the tested exact-online contract, not universal or theoretical shortest-path SOTA. A focused prior-art audit finds substantial precedent for the constituent repair and caching mechanisms, so we frame the contribution as systems integration, exactness/fairness discipline, operating-regime analysis and reproducible empirical evidence rather than broad algorithmic novelty.

The ratios in this abstract must always retain their named comparator and benchmark context. In particular, PR80's confidence intervals are slice-level robustness evidence rather than population inference.

## Suggested manuscript structure

1. **Introduction**
   - online exact shortest paths as a runtime-memory-maintenance tradeoff;
   - precise scoped research question;
   - contribution statement above;
   - explicit nonclaims.
2. **Problem Model and Contract**
   - directed nonnegative weighted fixed topology;
   - online mixed weight updates;
   - arbitrary point-to-point queries;
   - no future knowledge;
   - exactness and persistent-state budget.
3. **ADES Architecture**
   - cold bidirectional search;
   - source probation/admission;
   - resident SSSP/SPT state;
   - update repair and fallback;
   - bounded residency and controller.
4. **Correctness**
   - decrease filter/propagation;
   - non-tight and non-selected-parent increase filters;
   - selected-parent localization;
   - alternate-witness safety;
   - boundary repair and rebuild fallback.
5. **Prior Art and Positioning**
   - PR86 map; explicitly reject broad mechanism novelty.
6. **Experimental Methodology**
   - frozen contract;
   - exact oracles and trace identity;
   - DIRECT comparator qualification;
   - common logical-state accounting;
   - order balancing and uncertainty;
   - contextual comparator segregation.
7. **Results**
   - Boost PR62 narrow baseline result + correction;
   - VeloGraphX PR66 road/source-reuse;
   - VeloGraphX PR80 non-road cross-product;
   - memory observations and cache behavior;
   - contextual GraphBolt/NetworKit.
8. **Ablations and Negative Results**
   - PR57 v1 rejection/v2 redesign;
   - controller refutation;
   - other C5–C9 mechanism evidence without overclaiming.
9. **Limitations**
   - CPU-only tested environment;
   - tested graph/workload slices, no population generalization;
   - adapter-mediated VeloGraphX arbitrary-source semantics;
   - logical-state fairness is not equal RSS;
   - novelty remains narrow/unestablished.
10. **Reproducibility**
    - candidate/comparator SHAs;
    - canonical actions runs/artifacts/digests;
    - workload and oracle identities;
    - evidence manifests.
11. **Conclusion**
    - scoped empirical finding only.

## Recommended figures/tables

1. Architecture diagram: cold/probation/resident source lifecycle and update paths.
2. Runtime-memory contract diagram: common logical-state cap versus separate RSS observation.
3. PR66 source-regime table: total/query ratios and actual Velo cache hits.
4. PR80 graph × source-regime heatmap of VeloGraphX/ADES ratios.
5. Comparator qualification table: 16 gates × Boost/VeloGraphX.
6. Negative-evidence table: rejected PR57-v1, controller refutation, PR62 correction.
7. Prior-art mechanism map from PR86.

Do not collapse PR66 and PR80 cells into a population estimate across arbitrary graph families.

## Reproducibility index

| Package | Canonical identity |
|---|---|
| Publication contract | `docs/PUBLICATION_CONTRACT.md`, v1.0 |
| ADES candidate | `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe` |
| Boost holdout | run `38711826306`, artifact `11961213712`, SHA `0bd64d43...74029ff` |
| Velo qualification | PR64/65; upstream `2aa84ce...36075`; adapter blob `b09a9f51...fa6` |
| Velo NY holdout | run `38730045262`, artifact `11971060916`, SHA `c4a3d64a...d299` |
| Velo non-road holdout | run `38856829047`, artifact `12027088809`, SHA `617bb4c9...ef100` |
| Scoped-SOTA decision | PR81 |
| DIRECT registry closure | PR82 |
| Adversarial publication audit | PR83 + PR84 closure |
| Formal algorithm reconciliation | PR85 |
| Prior-art/novelty audit | PR86 |
| Publication package | PR87 |

Full unshortened identifiers are machine-readable in `evidence/pr87_publication_manifest.json`.

## Final submission gate

Before any external submission, require all of:

1. PR87 manifest and wording guard pass;
2. all central results trace to canonical immutable run/artifact identities;
3. PR62 source-family correction appears in the manuscript wherever PR62 is cited;
4. VeloGraphX is always described as adapter-mediated for arbitrary sources;
5. 64 MiB is described as a common cap/accounting contract, not equal observed memory;
6. PR80 uncertainty is described as nested-slice robustness, not population inference;
7. negative/rejected evidence is visible;
8. broad mechanism novelty is not claimed;
9. integrated-system novelty remains qualified as unestablished unless a later systematic review supports a narrower claim;
10. universal/theoretical SOTA wording is absent;
11. all named performance ratios retain comparator, workload scope and metric direction;
12. artifact/reproduction instructions are sufficient for a third party to reconstruct the central tables.

## PR87 verdict

The ADES repository is ready to transition from optimization/evidence generation to manuscript and artifact preparation.

The strongest defensible scientific position is now:

- **exactness/fairness package:** strong and reproducible;
- **scoped practical performance:** strong enough for `GO — SCOPED PRACTICAL SOTA` under the frozen contract;
- **broad algorithmic novelty:** rejected;
- **systems-integration originality:** potentially interesting but unestablished;
- **next work:** manuscript construction, independent artifact reproduction and venue-specific presentation—not another unconstrained performance rewrite.
