---
title: "ADES: Exact Adaptive Source Residency for Online Dynamic Shortest Paths Under Bounded State"
bibliography: references.bib
link-citations: true
---

# Abstract

Online shortest-path services face a persistent systems tradeoff. Recomputing an exact route from scratch keeps persistent state small but repeatedly pays graph-search cost; maintaining exact shortest-path state for many sources makes repeated queries cheap but consumes memory and adds update work. We study the middle regime and present **ADES**, an exact CPU architecture that adaptively moves arbitrary query sources between cold point-to-point search and byte-bounded resident single-source shortest-path (SSSP) state while processing online edge-weight increases and decreases without future knowledge. Resident states are maintained by exact local repair when its preconditions hold and by exact rebuild otherwise. Source admission is driven by observed cold-query work, while eviction combines reuse, recency, and update burden. The performance policy is therefore adaptive, but correctness does not depend on a predictor.

The evaluation is governed by a frozen contract covering trace identity, exactness, online causality, timing boundaries, comparator eligibility, and algorithm-owned persistent-state accounting. Against a pinned Boost.Graph exact-recomputation baseline, ADES achieves a 5.11x geometric-mean total-time advantage on a 24-cell New York road-graph holdout with zero loss cells. Against a pinned, unmodified VeloGraphX dynamic-maintenance implementation through a qualified byte-bounded arbitrary-source adapter, ADES achieves 15.56x on a fresh 24-cell New York source-reuse holdout and 157.68x on a preregistered 24-cell non-road holdout, again with 24/24 exact cells and no total- or query-time losses. A separate reproduction workflow re-downloads the canonical raw artifacts, verifies their hashes, ignores their archived aggregate verdicts, and independently regenerates these statistics from per-cell data.

These results support a **scoped practical runtime-memory frontier improvement under the tested exact-online contract**. They do not establish universal or theoretical shortest-path state of the art, population-level performance over arbitrary graph distributions, or broad algorithmic novelty. A focused prior-art audit finds substantial precedent for the constituent repair, caching, witness, and cost-aware control ideas; the contribution is therefore positioned as exact systems integration, operating-regime design, reproducible evaluation discipline, and empirical evidence.

# 1. Introduction

Shortest-path workloads are often presented as a choice between two extremes. At one extreme, a system stores the graph and answers each arbitrary `(source,target)` request by exact search. This approach naturally supports source churn and avoids maintaining source-specific state, but repeated queries from the same source repeatedly rediscover related distance information. At the other extreme, a system maintains exact shortest-path state rooted at one or more sources and repairs that state as the graph changes. Dynamic SSSP has a long history, including exact incremental and fully dynamic methods for edge insertions, deletions, and weight changes [@ramalingam1996incremental; @frigioni2000fully]. Maintaining many such source states, however, multiplies memory and update cost.

ADES targets the regime between these extremes. The key systems decision is not whether exact dynamic repair exists, but **which sources should currently deserve resident exact state under a hard state budget**. A queried source begins cold. Repeated or expensive cold work may justify promotion into a resident SSSP/SPT state. Resident states are kept exact under online weight changes by filters, local repair, bounded witness checks, or conservative rebuild. Sources that cease to justify their state can be evicted. The system remains exact because every policy decision chooses among exact execution paths; no policy score is trusted as a correctness oracle.

This leads to the paper's central question:

> Under a fixed persistent-state allowance, can an exact online system that adaptively materializes and maintains source-specific SSSP state improve the measured runtime-memory frontier over exact recomputation and qualified dynamic-maintenance comparators on arbitrary-source workloads with mixed weight increases and decreases?

The word **measured** is important. This is an empirical systems question under a frozen benchmark contract, not a claim of a new asymptotic dynamic-shortest-path bound.

## 1.1 Contributions

The paper makes six claim-bounded contributions.

1. **Exact bounded adaptive source residency.** ADES integrates cold exact bidirectional search, work-aware promotion, full exact per-source SSSP materialization, dynamic maintenance, conservative rebuild, and causal bounded eviction.
2. **A correctness-preserving repair architecture.** Edge-weight changes are filtered or repaired only under explicit shortest-path invariants; all uncertain or aborted cases fall back to exact recomputation.
3. **A reproducible fairness contract.** The central evidence freezes candidate/comparator identities, traces, exactness checks, timing boundaries, persistent-state accounting, online causality, and claim scope before definitive measurements.
4. **Qualified external comparisons.** The publication-grade DIRECT set contains Boost.Graph exact recomputation [@siek2002boost] and VeloGraphX through a transparent arbitrary-source adapter whose retained state and execution time are charged to the same logical-state contract.
5. **Cross-regime empirical evidence with negative results preserved.** The frozen candidate wins all admitted cells in the central Boost.Graph and VeloGraphX holdouts, while rejected optimizations, invalid interpretations, and a refuted controller heuristic remain part of the record.
6. **Independent recomputation of headline evidence.** A later evidence-only workflow fetches the pinned raw Actions artifacts, verifies ZIP identities, and recomputes exactness counts, geometric means, confidence intervals, minima, and loss-cell counts without trusting archived aggregate summaries.

We do **not** claim invention of dynamic SSSP repair, replacement witnesses, cache admission, maintenance-aware eviction, or measured repair-versus-recompute control. We also do not claim universal practical SOTA, equal process RSS, or a new worst-case complexity bound.

# 2. Problem Model and Evaluation Contract

## 2.1 Online graph model

Let `G=(V,E,w)` be a directed graph with fixed topology and nonnegative integer edge weights. The operation stream is online and consists of:

- `QUERY(s,t)`: return the exact shortest-path distance from arbitrary source `s` to target `t` under the current weights;
- `UPDATE(e,w')`: replace the weight of an existing stable edge identifier `e` by `w'`.

Both weight increases and decreases are permitted. The implementation receives operations in arrival order and has no knowledge of future operations. The publication experiments use weight changes rather than topology changes; therefore the empirical claim is scoped to that model even though related dynamic-shortest-path literature studies broader graph modifications [@ramalingam1996incremental; @frigioni2000fully].

For unreachable targets, the system returns the designated exact infinity representation used by the benchmark contract. All central experiments compare answers to an independent exact Dijkstra oracle before timing evidence is admitted.

## 2.2 Persistent-state resource model

The central matched comparisons bound **algorithm-owned persistent state**, not process RSS. The accounting contract includes retained graph/SSSP/cache state defined by each qualified implementation and excludes ephemeral allocator/runtime effects from the logical cap. RSS is reported separately.

For the matched VeloGraphX evidence, both implementations receive a maximum allowance of `67,108,864` bytes (64 MiB). This is a **common cap**, not a requirement that both consume equal actual bytes. In the non-road holdout, observed accounted peaks were approximately 65.51 MB for VeloGraphX and 1.80 MB for ADES. Those unequal occupancies are reported rather than normalized away.

## 2.3 Timing model

Primary algorithm time is decomposed as

`algorithm_ns = query_ns + update_ns`.

Graph parsing, deterministic input generation, oracle construction, and benchmark-harness setup are excluded from the primary algorithm-time ratio and retained as provenance. Query-only ratios are reported as a sensitivity view. Update-only ratios are diagnostic because the compared architectures decompose work differently.

# 3. ADES Architecture

## 3.1 Cold, probationary, and resident sources

ADES treats source identity as a managed resource.

**Cold source.** A source without resident state is answered by exact bidirectional point-to-point search. The current implementation uses integer-weight radix queues in the cold search path.

**Probationary source.** Cold searches accumulate causal evidence such as recurrence and scanned-edge work. This state is small relative to a full SSSP materialization.

**Resident source.** A promoted source owns a full exact distance vector and selected shortest-path tree. Point queries from that source become direct distance lookups, while graph updates trigger maintenance of the resident state.

The architecture deliberately avoids mandatory all-source preprocessing. Materialization happens only after online evidence indicates that a particular source may repay its state and maintenance cost.

## 3.2 Work-aware admission

A naive reuse policy could promote any repeated source after a fixed count. ADES instead incorporates the work already observed on cold queries. The intuition is economic: one expensive miss can justify materialization differently from one cheap miss, and recurrence is valuable only when future avoided work is likely to exceed resident update/space cost.

This principle is not claimed as a new caching concept. Frequency-based admission is well established [@einziger2017tinylfu], and later cache policies explicitly use measured access-time differences as an admission signal [@einziger2023access]. ADES applies the principle to exact shortest-path search work: cold scanned-edge effort becomes part of the evidence for materializing a full source state.

## 3.3 Bounded residency and eviction

Resident states are constrained by the persistent-state budget. When space is needed, the implementation ranks possible victims using causal observations that combine reuse, recency/age, and accumulated update burden. The specific heuristic is an engineering policy rather than a semantic rule.

This framing has established analogues in caching and materialized-state selection. Shortest-path caching uses query statistics and bounded placement [@jensen2012effective]; materialized-view selection explicitly balances query benefit against maintenance constraints [@gupta1999selection]. ADES differs in what the retained object is—a full exact source-rooted dynamic SSSP state—but does not claim to invent the general benefit-versus-maintenance tradeoff.

## 3.4 Exact handling of weight decreases

For a resident source with exact distance labels `d`, consider an edge `(u,v)` whose weight decreases to `w'`.

If `d[u] + w' >= d[v]`, the update cannot improve `v`, so the resident labels remain valid and no decrease propagation is required. Otherwise, the new candidate for `v` seeds Dijkstra-style improvement propagation. Because all weights are nonnegative, the propagation recomputes only strict decreases and terminates with exact labels for every improved vertex.

The high-level idea of incrementally repairing SSSP after weight changes is established prior art [@ramalingam1996incremental; @frigioni2000fully]. ADES's contribution is how these exact maintenance paths interact with source residency and online resource policy.

## 3.5 Exact handling of weight increases

Weight increases are more delicate because a previously shortest witness may disappear.

1. **Non-tight edge.** If the updated edge did not realize the old shortest distance to its head, increasing it cannot invalidate the selected distances.
2. **Tight but non-selected-parent edge.** If the edge was tight but was not the selected parent of its head, the selected shortest-path tree still supplies a complete unchanged shortest witness. The implementation therefore filters this case in O(1).
3. **Selected-parent edge.** If the increased edge is the selected tree parent, the implementation discovers the affected selected-tree subtree read-only. It either proves an alternate witness under the bounded witness logic, repairs the affected region from exact external-boundary labels, or rebuilds the source state exactly.

The predecessor/non-predecessor distinction has explicit pre-ADES precedent; transactional dynamic-graph work states that increasing a non-predecessor edge requires no incremental recomputation [@tripathi2018incremental]. Replacement/alternative witness techniques are likewise established in dynamic shortest paths [@king2001space]. ADES's bounded witness probe is therefore treated as an implementation specialization, not as invention of the concept.

## 3.6 Alternate-witness rescue and conservative rebuild

An earlier ADES variant eagerly probed for alternate witnesses. Although it improved targeted witness cases, it caused a statistically supported broad regression and was rejected. The surviving variant invokes the probe only when a selected-parent update has already produced a sufficiently large repair candidate, limits the incoming-arc scan, and accepts witnesses only under the proof conditions recorded in the algorithm specification.

If any repair precondition fails, a safety threshold aborts, or the controller chooses not to continue local repair, ADES performs exact full SSSP recomputation for that resident source. The performance controller therefore cannot turn an uncertain state into an approximate answer.

## 3.7 Repair-versus-rebuild control

The system records repair work and uses measured history to estimate whether local continuation or full recomputation is economically preferable. The campaign explicitly falsified the simpler rule “large affected subtree implies rebuild is cheaper”: structural damage can be large while the exact local repair is still cheaper than aborting and starting over.

Cost-model-based switching between incremental and static dynamic-graph processing has direct precedent [@bok2022cost]. ADES's contribution is the SSSP-specific telemetry, safety caps, and empirical operating regime, not the generic idea of cost-aware switching.

# 4. Correctness

The correctness argument is inductive over the online operation stream. Assume that immediately before an operation, every resident source stores exact distances for the current graph.

## 4.1 Queries

A cold query executes exact point-to-point search and returns the exact current distance. A resident query reads the exact resident label. Promotion first builds a full exact SSSP state; therefore promotion preserves the invariant rather than extrapolating from partial cold-search state.

## 4.2 Decreases

For a decrease `(u,v,w')`, if `d[u]+w' >= d[v]`, the edge cannot introduce a shorter path to `v`, so old labels remain exact. If the inequality is strict in the other direction, exact improvement propagation begins from the newly improved vertex. With nonnegative weights, standard label-setting propagation reaches every vertex whose distance decreases and cannot increase any exact label.

## 4.3 Increases

If the increased edge was not an old shortest witness, no old shortest path relied on it. If it was tight but not the selected parent, the unchanged selected tree supplies an explicit shortest witness to the head and its descendants until another selected-parent dependency is encountered. For a selected-parent increase, ADES identifies the selected-tree region whose witness may be invalid. Exact boundary distances from unaffected vertices provide repair seeds. The affected region is then either re-labeled exactly, discharged by a proved alternate witness, or rebuilt from scratch.

## 4.4 Policy independence

Admission, eviction, alternate-witness gating, and repair/rebuild prediction decide **which exact path to execute**. They do not decide what distance value is accepted. Every abort has an exact fallback. Thus a bad performance prediction can waste time but cannot silently change a shortest-path answer.

The central benchmark cells add a second safety layer: the operation stream is replayed through an independent exact Dijkstra oracle, and a cell is inadmissible if query answers disagree.

# 5. Related Work and Novelty Positioning

## 5.1 Dynamic shortest paths

Exact dynamic shortest-path maintenance predates ADES by decades. Ramalingam and Reps support heterogeneous batches of edge insertion, deletion, and length changes and derive incremental SSSP as a special case [@ramalingam1996incremental]. Frigioni, Marchetti-Spaccamela, and Nanni maintain distances and shortest-path trees under fully dynamic edge updates [@frigioni2000fully]. King and Thorup develop space-saving machinery for directed dynamic transitive closure and shortest-path structures [@king2001space]. These works rule out a defensible claim that ADES invents dynamic SSSP maintenance.

Tripathi et al. further provide an explicit dynamic-query precedent for treating an increased non-predecessor edge as requiring no incremental recomputation [@tripathi2018incremental]. Consequently, ADES's constant-time non-selected-parent filter is best viewed as a correct and practically important specialization, not a new conceptual rule.

## 5.2 Shortest-path caching and maintained query state

Caching shortest paths under bounded capacity is also established. Jensen, Thomsen, and Yiu estimate benefit from query logs and greedily place useful shortest paths in a cache [@jensen2012effective]. Li et al. study how a shortest-path cache should be refreshed when the underlying graph changes [@li2017refreshment]. These systems do not match the exact ADES object model, but they defeat any broad “first shortest-path cache” claim.

Ammar et al. examine differential maintenance of recursive graph queries and explicitly trade memory for incremental maintenance by dropping retained differences and recomputing them when necessary [@ammar2022optimizing]. That work is broader than SSSP-specific source residency but directly reinforces the general memory-versus-maintenance theme.

## 5.3 Admission, eviction, and materialization economics

TinyLFU shows the value of using recent access frequency as an admission signal [@einziger2017tinylfu]. Access-time-aware caching later incorporates measured miss/hit cost into policy [@einziger2023access]. Gupta and Mumick study selecting materialized state under maintenance-cost constraints [@gupta1999selection]. These precedents mean that ADES's reuse/recency/update-debt policy and work-aware admission are engineering combinations of known systems principles rather than standalone algorithmic novelty claims.

## 5.4 Repair-versus-recompute selection

Bok et al. use a dynamic-graph cost model to choose between incremental and static processing from measured/history-based costs [@bok2022cost]. ADES's repair-versus-rebuild controller belongs to this same conceptual family, specialized to exact SSSP and instrumented with SSSP-specific work counters.

## 5.5 Integrated architecture and contemporary collision

The focused prior-art audit did not identify one pre-ADES system that simultaneously combines arbitrary online sources, cold exact point-to-point search, full exact source-level SSSP materialization, mixed weight-increase/decrease repair, a hard auditable byte cap, causal source admission/eviction, bounded witness rescue, and conservative exact rebuild. **Failure to find such a system is not proof of novelty.** Nearly every ingredient has substantial prior art, and the audit is not a systematic review of every routing, database, caching, and dynamic-graph venue.

A 2026 contemporary preprint, ALSSSP, independently describes adaptive hot-source caching including complete SSSP trees and bounded retention [@rai2026alsssp]. Its update semantics differ, and the present audit does not resolve priority between integrated systems. We therefore retain the conservative classification: **broad algorithmic novelty is rejected; integrated-system originality remains unestablished and possibly distinct.**

# 6. Experimental Methodology

## 6.1 Frozen candidate and external comparators

The publication candidate is frozen at ADES commit

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`.

The central DIRECT comparator set contains two external implementations.

**Boost.Graph Dijkstra recomputation.** Boost.Graph 1.92.0 supplies an exact arbitrary-source recomputation baseline [@siek2002boost]. It retains no cross-query SSSP state. The archived source package has SHA-256 `5c1d40cb8e19adbf740a4ec2da35b3e58f3f5804b1dce44deb53df72193cbc6c` in the VeloGraphX holdout lineage.

**VeloGraphX weighted SSSP.** Upstream VeloGraphX is pinned at commit `2aa84ce81a180487239c661b65639e5100136075` and is not algorithmically modified. Upstream behavior is fixed-source, so arbitrary-source evaluation is supplied by a transparent qualified adapter: an online byte-bounded LRU of independent VeloGraphX source instances. Adapter execution and retained graph/SSSP state are fully charged. Therefore every manuscript statement about arbitrary-source VeloGraphX is explicitly **adapter-mediated**, not a claim that upstream VeloGraphX natively exposes the same semantics.

NetworKit and GraphBolt experiments exist in the evidence archive, but their semantic/implementation assumptions differ from the complete headline contract. They are contextual evidence and are not used to manufacture the central C-SOTA conclusion.

## 6.2 Outcome-neutral experiment discipline

The campaign separates benchmark integrity from performance outcome. A completed experiment may support or falsify a hypothesis without making CI fail. CI fails on semantic defects—wrong graph/trace identity, exactness disagreement, accounting violation, malformed output, future knowledge, or a broken execution contract—not merely because ADES loses.

This distinction matters because negative branches did occur: an eager witness optimization regressed the broad matrix, a structural repair/rebuild rule was refuted, one early PR80 run failed before timing due to a missing output directory, and an early interpretation of PR62 overstated source-family breadth. These events remain in the evidence history.

## 6.3 New York road-graph holdouts

Two central road-graph experiments are retained for different purposes.

**PR62: Boost.Graph recomputation.** The experiment contains 24 exact matched cells and supports the narrower claim that ADES outperformed exact external recomputation on the executed New York traces. The traces labeled as non-uniform families had `locality_percent=0`; therefore PR62 does **not** establish source-policy breadth. That interpretation was corrected rather than post-hoc relabeling the data.

**PR66: VeloGraphX source reuse.** This fresh 24-cell holdout was designed to test actual source reuse against the qualified dynamic-maintenance comparator. Its source regimes include uniform, two-source hot pool, four-source hot pool, and rotating four-source hot pools. VeloGraphX records 1,164 cache hits across the two hot-pool families and another 576 in rotating-hot cells, demonstrating that the comparator's retained-source path was genuinely exercised.

## 6.4 Preregistered non-road holdout

PR80 tests the strongest remaining interaction before evidence synthesis: `VeloGraphX x non-road topology x arbitrary sources x source reuse` under the same logical-state cap.

The topology family is deterministic Barabasi-Albert-style preferential attachment:

- 15,000 vertices;
- five attachments per new vertex;
- 74,985 logical undirected edges;
- positive initial integer weights in `[100,1000]`;
- symmetric directed representation for execution.

Three fresh graph seeds were frozen before timing: `8209`, `9103`, and `10037`. Two fresh workload seeds were frozen: `12007` and `13001`. Each graph/workload-seed pair is crossed with four arbitrary-source regimes:

- `uniform`: 60 distinct sources, zero source reuse;
- `hot-pool-2`: queries from a two-source pool, reuse fraction at least 0.80;
- `hot-pool-4`: queries from a four-source pool, reuse fraction at least 0.80;
- `rotating-hot-4`: disjoint four-source pools rotate every 30 queries, reuse fraction at least 0.80.

Each cell contains 60 queries and 12 logical weight replacements, one every five queries, alternating decrease and increase. This gives `3 x 2 x 4 = 24` preregistered cells.

Each cell has two measured matched repetitions with reversed execution order: VeloGraphX then ADES, followed by ADES then VeloGraphX. The cell ratio is the geometric mean of the two order-balanced repetition ratios.

## 6.5 Exactness and identity gates

A central cell is admitted only when all frozen integrity gates pass, including:

- identical graph and trace SHA-256 identities across implementations;
- identical operation, query, and update counts;
- online arrival order with no future knowledge;
- independent exact-oracle agreement;
- declared timing decomposition;
- valid persistent-state accounting within the common cap;
- pinned implementation/adapter identities.

## 6.6 Statistical summary

For each admitted cell, the primary ratio is comparator `algorithm_ns / ADES algorithm_ns`; query sensitivity is comparator `query_ns / ADES query_ns`. Across the 24 cells, multiplicative effects are summarized by the geometric mean. Two-sided 95% Student-t intervals are computed in log-ratio space over the 24 cell-level ratios.

These intervals characterize the **declared experimental slice**. They are not population-level confidence intervals over arbitrary graph or workload distributions. We additionally report individual loss-cell counts and minima because an aggregate geometric mean can conceal important losing regimes.

# 7. Results

## 7.1 Boost.Graph exact recomputation on New York traces

| Metric | Boost.Graph / ADES |
|---|---:|
| Total algorithm-time geometric mean | **5.112681x** |
| Total 95% interval | `[4.851231, 5.388222]` |
| Query-time geometric mean | **5.059393x** |
| Query 95% interval | `[4.800065, 5.332731]` |
| Exact cells | `24/24` |
| Total-time loss cells | `0` |
| Query-time loss cells | `0` |
| Minimum total cell ratio | `4.073915x` |
| Minimum query cell ratio | `4.032328x` |

The result strongly supports a performance advantage over exact recomputation on this frozen slice. It does **not** support the original source-family breadth interpretation because every PR62 workload record has `locality_percent=0`. The manuscript uses PR62 only for the external-recomputation result.

## 7.2 VeloGraphX on fresh New York source reuse

| Metric | VeloGraphX / ADES |
|---|---:|
| Total algorithm-time geometric mean | **15.561191x** |
| Total 95% interval | `[14.569887, 16.619942]` |
| Query-time geometric mean | **3.897866x** |
| Query 95% interval | `[2.982019, 5.094991]` |
| Exact cells | `24/24` |
| Total-time loss cells | `0` |
| Query-time loss cells | `0` |
| Minimum total cell ratio | `11.298778x` |
| Minimum query cell ratio | `2.290465x` |

The reuse concern is directly exercised rather than inferred from labels. Recomputed VeloGraphX cache hits are `588` in `hot-pool-2`, `576` in `hot-pool-4`, `576` in `rotating-hot-4`, and `0` in `uniform`. The maximum VeloGraphX accounted state in this holdout is `55,185,520` bytes, below the 64 MiB logical-state cap.

## 7.3 VeloGraphX on preregistered non-road generalization

| Metric | VeloGraphX / ADES |
|---|---:|
| Total algorithm-time geometric mean | **157.677417x** |
| Total 95% slice-level interval | `[95.329154, 260.803405]` |
| Query-time geometric mean | **30.199379x** |
| Query 95% slice-level interval | `[16.218261, 56.233063]` |
| Exact cells | `24/24` |
| Total-time loss cells | `0` |
| Query-time loss cells | `0` |
| Minimum total cell ratio | `30.153080x` |
| Minimum query cell ratio | `6.797390x` |

All frozen source-family and graph-instance lower interval bounds exceed one. VeloGraphX records `996` cache hits, `444` misses, and `204` evictions, so the retained-state path is active. Peak accounted state is `65,507,832` bytes for VeloGraphX and `1,802,576` bytes for ADES, both under the common 64 MiB cap.

The magnitude of the non-road ratio should be interpreted within this exact comparator/workload contract. It is not evidence that ADES is hundreds of times faster than every dynamic shortest-path implementation or on arbitrary graph distributions.

## 7.4 Evidence synthesis

The three experiments answer different threats.

- PR62 establishes that the result is not limited to an internal ADES baseline: ADES beats pinned external exact recomputation on the executed road slice.
- PR66 establishes a favorable result against a qualified dynamic-maintenance comparator while genuinely exercising source reuse on road data.
- PR80 preregisters fresh non-road graph/workload seeds and shows the VeloGraphX result survives a topology-family change across zero-reuse and high-reuse regimes.

Together, these results support the repository's bounded statement: **scoped practical SOTA evidence under Publication Contract v1.0 for the admitted exact-online contract and qualified DIRECT comparator set.** The statement is intentionally narrower than universal or theoretical SOTA.

# 8. Negative Results and Mechanism Evidence

## 8.1 Eager alternate-witness probing was rejected

PR57-v1 broadened alternate-witness probing and helped targeted cases, but the broad matrix showed a supported regression: `PR55 / PR57-v1 = 0.910073x`, 95% interval `[0.872704, 0.949041]`. The branch was rejected rather than rationalized after the fact.

The surviving v2 policy invokes the bounded probe only in already-large repair situations. This is a useful systems result: a logically safe optimization can still be globally harmful when its check cost is placed on the wrong frequency path.

## 8.2 Affected-region size is not repair economics

A second hypothesis treated a large affected selected-tree region as a proxy for “rebuild will be cheaper.” Experiments partially refuted that rule. Structural damage size and actual repair cost are related but not interchangeable; boundary structure and successful local propagation can make a large region comparatively cheap. The controller therefore uses more direct work/cost telemetry.

## 8.3 Interpretation and harness failures are retained

Three non-performance corrections are part of the evidence chain.

- PR62's labeled source-family breadth was retracted after discovering `locality_percent=0` in the executed traces.
- PR80's first attempt failed before comparative timing because the harness lacked an output directory. It generated no admitted performance evidence and was repaired without changing the frozen scientific contract.
- PR83/84 adversarial publication checks initially contained brittle exact-string predicates. Those predicates were repaired semantically while the scientific boundary conditions remained unchanged.

These cases are included because reproducibility depends on distinguishing a scientific falsification from an instrumentation or audit-harness defect.

# 9. Independent Artifact Reproduction

After the submission evidence package was frozen, PR88 added an evidence-only reproduction path. Its verifier re-downloads the exact canonical GitHub Actions artifact ZIPs by artifact ID, checks each SHA-256 against the publication manifest, and recomputes the central statistics from per-cell CSV rows. It deliberately ignores each artifact's archived top-level aggregate summary.

The canonical PR88 run is `38865585699`. Its retained reproduction artifact is ID `12030529254` with digest

`sha256:edb03af36c4c6188866c0ec10a86c2394ea95ea8d185e93ee375a224534b1ee5`.

The reproduction path re-established:

- all three `24/24` exact-cell counts;
- all three total/query geometric means;
- the log-space 95% t intervals;
- zero total/query loss cells;
- per-experiment minima;
- PR62's `locality_percent=0` correction;
- PR66 cache-hit and state-cap facts;
- PR80 seed sets, source regimes, cache counters, and peak accounted state.

The recomputation result matches the publication manifest to floating-point/recorded-rounding precision. This is stronger provenance than copying a summary table, but it is **not an external independent replication by a different research group or hardware environment**.

# 10. Threats to Validity and Limitations

## 10.1 Scope of graph dynamics

The benchmark model changes weights on a fixed topology. Results should not be extrapolated to arbitrary edge insertion/deletion streams without new experiments, even though prior dynamic-SP literature covers those operations [@ramalingam1996incremental; @frigioni2000fully].

## 10.2 Comparator semantics

The VeloGraphX arbitrary-source comparison is adapter-mediated because upstream VeloGraphX is fixed-source. The adapter is transparent, causal, byte-bounded, and fully charged, but it remains a systems wrapper. We therefore do not claim superiority over a hypothetical native arbitrary-source VeloGraphX design.

## 10.3 Logical state versus RSS

The common 64 MiB contract bounds declared algorithm-owned persistent state. It does not imply equal process RSS or equal actual memory occupancy. Allocator behavior, runtime libraries, and transient memory are separate concerns.

## 10.4 Experimental population

PR80's t intervals describe a preregistered 24-cell slice over three deterministic preferential-attachment instances, four source regimes, and two workload seeds. They do not justify population-level inference over all non-road graphs. The road evidence similarly comes from declared New York slices.

## 10.5 Large effect sizes

The very large PR80 ratios are real measurements under the frozen contract, but large ratios can be especially architecture- and workload-sensitive. The manuscript therefore reports minima, loss cells, cache behavior, state occupancy, and the adapter semantics alongside aggregate ratios.

## 10.6 Novelty

The constituent mechanisms have substantial prior art. The focused audit rejected broad mechanism novelty and did not prove novelty of the integrated architecture. The paper's contribution does not depend on being first to invent dynamic repair, cache admission, alternate witnesses, or repair-versus-recompute control.

## 10.7 External replication

PR88 provides an independent recomputation path over immutable archived artifacts, not independent third-party replication. A stronger future validation would rebuild the pinned implementations and execute the frozen workloads on separately administered hardware.

# 11. Reproducibility and Artifact Map

The repository preserves the full evidence lineage rather than only a final table.

**Frozen ADES candidate**

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

**Canonical central evidence**

| Evidence | Workflow run | Artifact ID | ZIP SHA-256 |
|---|---:|---:|---|
| PR62 Boost.Graph NY | `38711826306` | `11961213712` | `0bd64d43ba48fda4c9e0752ea6acafc1c9fa8ddb7eec2f2fdabc88d1574029ff` |
| PR66 VeloGraphX NY reuse | `38730045262` | `11971060916` | `c4a3d64af948afd03daa1e983ee2d75eadf838ca94bdc59103c25ff5575dd299` |
| PR80 VeloGraphX non-road | `38856829047` | `12027088809` | `617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100` |
| PR88 independent recomputation | `38865585699` | `12030529254` | `edb03af36c4c6188866c0ec10a86c2394ea95ea8d185e93ee375a224534b1ee5` |

The machine-readable publication index is `evidence/pr87_publication_manifest.json`. The independent per-cell recomputation implementation is `tools/reproduce_publication_evidence.py`. The comparator registry is `benchmarks/comparators/registry.json`. The final reconciled algorithm description is `docs/ALGORITHM.md`. The performance claim boundary is recorded in `docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md`, the adversarial publication audit in PR83/84, the algorithm/source reconciliation in PR85, and the prior-art/novelty classification in PR86.

The historical `paper/ADES_MANUSCRIPT_SCAFFOLD.md` is intentionally retained unchanged as the audited pre-PR88 manuscript scaffold. This document is the citation-complete submission surface built on top of that lineage.

# 12. Conclusion

ADES explores a practical middle regime for exact online shortest paths: maintain expensive source-specific state only when observed workload economics justify it, keep that state exact under graph-weight changes, and bound the retained state explicitly. The architecture combines cold bidirectional search, adaptive source materialization, exact dynamic repair, conservative rebuild, and bounded causal retention.

Under the frozen publication contract, the candidate shows no admitted total- or query-time loss cells across the three central 24-cell external-comparator experiments. It achieves 5.11x total-time geometric-mean speedup over Boost.Graph recomputation on the retained New York slice, 15.56x over the qualified VeloGraphX adapter on fresh New York source-reuse workloads, and 157.68x on a preregistered non-road holdout. A separate evidence-only workflow re-downloads the pinned raw artifacts and independently regenerates the reported statistics from cell-level data.

The appropriate conclusion is deliberately bounded. The evidence supports a **scoped practical runtime-memory frontier improvement under the declared exact-online contract and qualified comparator set**. It does not establish universal practical superiority, a new asymptotic shortest-path bound, population-level performance on arbitrary graphs, or broad algorithmic novelty. The strongest research contribution is the integration of exact mechanisms into an adaptive bounded-residency system, the explicit operating-regime and negative-result analysis, and an evidence process designed to make favorable and unfavorable outcomes equally auditable.
