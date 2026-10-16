# ADES: Exact Adaptive Source Residency for Online Dynamic Shortest Paths Under Bounded State

> **Manuscript scaffold — claim-bounded by Publication Contract v1.0, PR83/84 adversarial audit, and PR86 prior-art audit.**

## Abstract

Online shortest-path services face a tension between recomputing exact routes on demand and retaining source-specific state that accelerates repeated queries but incurs memory and update cost. We present ADES, an exact CPU architecture that adaptively moves query sources between cold point-to-point search and bounded resident single-source shortest-path state while processing online edge-weight increases and decreases without future knowledge. ADES uses exact local repair when valid and exact recomputation as a conservative fallback, and exposes auditable algorithm-owned persistent-state accounting for matched evaluation. We freeze the research question, comparator eligibility, timing boundaries, and memory contract before the definitive experiments. Against a pinned Boost.Graph exact-recomputation baseline, ADES is 5.11× faster in total algorithm time on a 24-cell NY holdout, with no loss cells. Against a pinned VeloGraphX dynamic-maintenance implementation through a qualified byte-bounded arbitrary-source adapter, ADES is 15.56× faster on a fresh 24-cell NY source-reuse holdout and 157.68× faster on a preregistered 24-cell non-road holdout; all admitted cells pass independent exact-oracle checks and the latter holdout has no total- or query-time losses. These results support a scoped practical runtime-memory frontier improvement under the tested exact-online contract, not universal or theoretical shortest-path SOTA. A focused prior-art audit finds substantial precedent for the constituent repair and caching mechanisms, so we frame the contribution as systems integration, exactness/fairness discipline, operating-regime analysis, and reproducible empirical evidence rather than broad algorithmic novelty.

## 1. Introduction

Exact online shortest-path systems have two expensive extremes. A system can answer each arbitrary-source query from scratch, paying repeated graph-search cost but retaining little query-specific state. Or it can maintain shortest-path state for many sources, making repeated queries cheap at the price of persistent memory and update maintenance.

ADES studies the middle regime: **bounded adaptive residency** of exact per-source shortest-path state. A queried source begins cold. Repeated or expensive cold work can justify promotion into resident SSSP state; resident sources are maintained through exact dynamic repair or conservative rebuild and are evicted under a bounded persistent-state policy.

The paper asks a deliberately scoped question:

> Can adaptive bounded residency of exact single-source shortest-path state improve the runtime-memory Pareto frontier for online shortest-path workloads containing arbitrary edge-weight increases/decreases and repeated-source queries, without future knowledge?

### Contributions

This paper claims the following contributions, and no broader ones:

1. **Exact bounded-residency architecture.** ADES integrates exact cold point-to-point search, source promotion, resident SSSP maintenance, exact dynamic repair, exact rebuild fallback, and bounded causal source eviction.
2. **Reproducible fairness contract.** The evaluation freezes exactness, trace identity, persistent-state accounting, timing boundaries, comparator eligibility, and claim scope before definitive experiments.
3. **Qualified external comparison.** Boost.Graph exact recomputation and adapter-mediated VeloGraphX dynamic maintenance both satisfy the frozen DIRECT-comparator gates.
4. **Cross-regime empirical evidence.** The frozen ADES candidate outperforms those DIRECT comparators on the admitted NY and non-road slices, including genuine high source reuse.
5. **Negative-evidence discipline.** Rejected mechanism variants, invalid benchmark interpretations, controller refutations, and contextual comparator mismatches remain visible and constrain the conclusions.
6. **Conservative prior-art positioning.** A focused audit rejects broad novelty claims for the constituent repair/caching mechanisms and leaves integrated-system novelty unestablished.

We do **not** claim a new worst-case dynamic shortest-path bound, universal practical SOTA, equal RSS, or invention of dynamic SSSP repair/cost-aware caching.

## 2. Problem Model

Let `G=(V,E,w)` be a directed graph with fixed topology and nonnegative integer edge weights. The online stream contains:

- `QUERY(s,t)`: return the exact shortest-path distance from arbitrary source `s` to target `t`;
- `UPDATE(e,w')`: change the weight of an existing stable edge ID, including both increases and decreases.

The system receives no future operation knowledge.

The evaluation treats algorithm-owned persistent state as a bounded resource. Process RSS is separately disclosed but is not substituted for the logical-state contract.

## 3. ADES Architecture

### 3.1 Source lifecycle

Each source is conceptually cold, probationary, or resident.

- **Cold:** exact bidirectional search answers the query.
- **Probationary:** cold-query recurrence/work is accumulated.
- **Resident:** a full exact SSSP/SPT state is maintained and point queries become constant-time distance lookups.

### 3.2 Admission and eviction

Admission uses observed cold-search work rather than recurrence alone. Residency is bounded, and victim selection combines reuse/recency with accumulated update burden. These policies affect performance only; exactness is protected by exact cold/rebuild paths.

### 3.3 Dynamic maintenance

For resident sources:

- irrelevant decreases are filtered; relevant decreases seed exact Dijkstra-style improvement propagation;
- non-tight increases are filtered;
- a tight increased edge that is not the selected parent witness is filtered because the selected SPT still supplies a complete unchanged shortest-path witness;
- selected-parent increases discover the affected selected-tree subtree read-only and either repair it from exact external boundary labels or rebuild;
- large repairs may use the bounded PR57-v2 alternate-witness probe before invalidation;
- all abort paths fall back to exact full SSSP recomputation.

The implementation and proof sketch are frozen in `docs/ALGORITHM.md` after PR85 source/spec reconciliation.

## 4. Correctness Argument

The correctness proof is inductive over online operations.

1. Cold queries use exact bidirectional shortest-path search.
2. Resident queries read an exact resident label.
3. Promotion builds exact SSSP state.
4. Decrease filters are safe when the updated edge cannot improve its head; relevant decreases propagate only strict improvements.
5. Increase filters are safe when the edge was not an old shortest witness or is absent from the selected witness tree.
6. A selected-parent increase either retains a proved independent alternate witness, repairs the affected subtree from exact boundary labels, or rebuilds exactly.
7. Admission/eviction/controller choices select among exact execution paths and cannot change semantic answers.

Every central performance query is additionally checked against the experiment's independent exact oracle.

## 5. Related Work and Positioning

ADES builds on extensive prior work rather than claiming invention of its ingredients.

### 5.1 Dynamic shortest paths

Ramalingam and Reps (1996) developed incremental shortest-path machinery supporting heterogeneous graph modifications including edge-length changes. Frigioni, Marchetti-Spaccamela, and Nanni developed fully dynamic shortest-path-tree algorithms under insertions/deletions/weight changes. These lines of work defeat any claim that ADES invented exact dynamic SSSP repair.

### 5.2 Unaffected/non-tree updates and replacement witnesses

Pre-ADES dynamic-query work explicitly distinguishes predecessor from non-predecessor updates, including the case where increasing a non-predecessor edge requires no incremental recomputation. Dynamic shortest-path structures also have a long history of replacement/alternative witness logic. ADES's selected-witness filter and PR57 bounded witness probe are therefore engineering choices, not broad conceptual novelty claims.

### 5.3 Shortest-path caching and bounded maintained state

Shortest-path caching, cache refresh under graph changes, and memory-bounded retention of incremental query state are established topics. ADES differs operationally by combining arbitrary online source queries, cold exact search, source-level full SSSP materialization, dynamic exact maintenance, and a hard auditable state budget, but the focused review does not establish originality of that combination.

### 5.4 Cost-aware admission/retention and repair/recompute control

TinyLFU-style admission, cost-aware caching, maintenance-cost-constrained materialization, and dynamic-graph cost models all establish the general principles of retaining state based on expected value and selecting incremental processing versus recomputation based on measured/estimated cost.

Accordingly, our contribution is positioned as **systems integration, exactness/fairness methodology, operating-regime analysis, and empirical runtime-memory performance**.

## 6. Experimental Methodology

### 6.1 Frozen contract

The publication contract was frozen before definitive evidence. Central matched cells require:

- exact independent-oracle agreement;
- identical operation streams and cryptographic identities;
- online arrival order and no future knowledge;
- declared timing boundaries;
- persistent-state accounting and budget enforcement;
- pinned external implementations;
- preserved negative/loss cells.

### 6.2 DIRECT comparators

**Boost.Graph Dijkstra recomputation.** Exact arbitrary-source cold baseline with no retained shortest-path state.

**VeloGraphX weighted SSSP.** Dynamic-maintenance comparator using unmodified pinned upstream VeloGraphX. Because upstream is fixed-source, the qualified comparison uses a transparent causal byte-bounded LRU of independent source instances. This arbitrary-source behavior is adapter-mediated and is fully charged to timing/state accounting.

### 6.3 Statistics

Matched order-balanced repetitions are reduced to cell-level ratios. Geometric means summarize multiplicative speedups. Reported confidence intervals characterize the declared experimental slice. They are not treated as population-level inference over arbitrary graphs/workloads.

## 7. Results

### 7.1 Boost.Graph: fresh NY traces

| Metric | Boost / ADES |
|---|---:|
| Total algorithm-time GM | 5.112681× |
| Total 95% CI | [4.851231, 5.388222] |
| Query-time GM | 5.059393× |
| Query 95% CI | [4.800065, 5.332731] |
| Exact cells | 24/24 |
| Total/query loss cells | 0 / 0 |

**Interpretation correction.** The executed PR62 traces are valid, but the originally labeled non-uniform source families used `locality_percent=0` and therefore do not establish source-policy breadth. We use PR62 only for the narrower external-recomputation result.

### 7.2 VeloGraphX: fresh NY source reuse

| Metric | VeloGraphX / ADES |
|---|---:|
| Total algorithm-time GM | 15.561191× |
| Total 95% CI | [14.569887, 16.619942] |
| Query-time GM | 3.897866× |
| Query 95% CI | [2.982019, 5.094991] |
| Exact cells | 24/24 |
| Total/query loss cells | 0 / 0 |

The hot-pool cells generated 1,164 VeloGraphX cache hits and rotating-hot cells another 576, directly falsifying the concern that the favorable result depended only on source churn with no reuse.

### 7.3 VeloGraphX: fresh non-road generalization

| Metric | VeloGraphX / ADES |
|---|---:|
| Total algorithm-time GM | 157.677417× |
| Total 95% slice-level interval | [95.329154, 260.803405] |
| Query-time GM | 30.199379× |
| Query 95% slice-level interval | [16.218261, 56.233063] |
| Exact cells | 24/24 |
| Total/query loss cells | 0 / 0 |
| Worst total cell | 30.153080× |
| Worst query cell | 6.797390× |

All source-family and graph-instance lower bounds exceed one. VeloGraphX produced 996 cache hits, so the retained-state path was exercised.

The common state cap was 64 MiB. Observed accounted peaks were about 65.51 MB for VeloGraphX and 1.80 MB for ADES; the contract is a common maximum allowance, **not** equal memory occupancy. RSS is separately reported.

## 8. Negative Results and Ablations

### 8.1 Eager witness probing failed

PR57-v1 improved targeted witness cases but caused a supported regression in the broad matrix (`PR55/PR57-v1=0.910073×`, 95% CI `[0.872704,0.949041]`). It was rejected. PR57-v2 gates the probe to already-large repairs and preserved targeted wins without a supported broad regression.

### 8.2 Structural damage is not economic damage

The controller hypothesis “large affected subtree implies rebuild is cheaper” is refuted as a general rule. Large structural damage can still be cheaper to repair than abort and rebuild. Remaining repair cost must be estimated directly.

### 8.3 Benchmark/audit corrections remain part of the evidence

The PR62 source-family correction, PR80 pre-timing directory failure, and PR83/84 audit-predicate repairs are retained in the archive. They demonstrate why outcome-neutral harness validation and interpretation audits matter.

## 9. Limitations

- The definitive evidence is CPU-based and covers declared workload slices rather than arbitrary graph populations.
- VeloGraphX arbitrary-source behavior is adapter-mediated because upstream is fixed-source.
- The logical persistent-state budget is not equal RSS and not equal actual state occupancy.
- GraphBolt and NetworKit evidence is assumption-separated/contextual for the complete C-SOTA scope.
- The large non-road ratios should not be interpreted beyond their comparator/workload contract.
- Broad algorithmic novelty is rejected; integrated-system originality remains unestablished.
- The study does not establish a new asymptotic bound.

## 10. Reproducibility

The submission artifact should expose:

- frozen candidate and comparator SHAs;
- comparator registry and qualification evidence;
- graph/trace hashes;
- independent oracle records;
- raw paired timing outputs;
- state-accounting reports and RSS observations;
- scripts that regenerate aggregate tables;
- canonical workflow run/artifact identifiers;
- PR87 publication manifest.

The machine-readable index is `evidence/pr87_publication_manifest.json`.

## 11. Conclusion

ADES demonstrates that bounded adaptive residency of exact source-specific shortest-path state can materially improve the measured runtime-memory tradeoff under the tested exact-online workloads. The result survives independent exact-oracle validation, a frozen common state-accounting contract, a qualified dynamic-maintenance comparator, genuine source reuse, and road/non-road topology changes. The evidence supports a **scoped practical SOTA** claim under the declared benchmark contract. It does not support universal or theoretical SOTA, and the constituent algorithmic mechanisms have substantial prior art. The strongest contribution is therefore the integrated exact architecture, reproducible evaluation discipline, operating-regime analysis, and empirical runtime-memory result.

## References to include in the full manuscript

At minimum:

- Ramalingam & Reps (1996), DOI `10.1006/JAGM.1996.0046`.
- Frigioni, Marchetti-Spaccamela & Nanni (2000), DOI `10.1006/jagm.1999.1048`.
- King & Thorup (2001), DOI `10.1007/3-540-44679-6_30`.
- Tripathi et al. (2018), DOI `10.1002/cpe.4500`.
- Jensen, Thomsen & Yiu (2012), DOI `10.1145/2213836.2213872`.
- Li et al. (2017), DOI `10.1016/j.eswa.2016.08.011`.
- Ammar et al. (2022), DOI `10.14778/3551793.3551862`.
- TinyLFU (2017), DOI `10.1145/3149371`.
- cost-aware cache admission, DOI `10.1145/3572778`.
- Bok et al. (2022), DOI `10.3390/electronics11040660`.

Before submission, bibliographic metadata and citation formatting should be revalidated against primary publisher/library records.
