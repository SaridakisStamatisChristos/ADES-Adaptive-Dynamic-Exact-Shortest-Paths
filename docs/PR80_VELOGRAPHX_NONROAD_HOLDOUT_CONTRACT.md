# PR80 — preregistered VeloGraphX non-road arbitrary-source dynamic holdout

## Status

**PREREGISTERED BEFORE PR80 TIMING.**

PR80 attacks the strongest remaining cross-product gap in the ADES practical-SOTA evidence matrix. Prior evidence already includes:

- qualified DIRECT VeloGraphX dynamic-maintenance comparisons on frozen NY road graphs with arbitrary-source source-reuse workloads (PR65/PR66);
- qualified NetworKit dynamic-maintenance comparisons on a non-road preferential-attachment family (PR67/PR68);
- fixed-source GraphBolt historical-Cilk comparisons on real email and Gnutella directed topologies (PR78/PR79).

What remains under-tested is **VeloGraphX × non-road topology × arbitrary sources × source reuse under equal logical-state budget**. PR80 tests exactly that interaction without changing the frozen ADES candidate, VeloGraphX upstream, adapter, pair driver, logical-state budget, or statistical direction after outcomes are observed.

A completed mixed/loss result is accepted evidence. CI must not encode a preferred winner.

## 1. Frozen implementation identities

### ADES candidate

- candidate commit: `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`
- expected profile: `ADES-V3`
- expected logical accounting: version 2

The candidate is frozen from the PR60–PR68 comparison lineage. PR80 does not tune ADES against the new non-road cells.

### VeloGraphX

- upstream repository: `sauravsingla/VeloGraphX`
- upstream commit: `2aa84ce81a180487239c661b65639e5100136075`
- upstream algorithm modification: **none permitted**
- adapter: existing qualified `benchmarks/velographx_direct.cpp`
- adapter blob identity on the PR80 lineage: must equal `b09a9f51b2b0300dce2801d0a8afaa49440d45a6`
- matched pair driver: existing `tools/run_pr65_velographx_head_to_head.py`
- pair-driver blob identity: `44019d478377282b5056a6b07d86d76757f7efc0`

The adapter supplies arbitrary-source behavior as a byte-bounded LRU of independent VeloGraphX fixed-source instances. All retained graph + SSSP state is charged to the logical-state budget. Upstream VeloGraphX remains unchanged.

### Independent oracle

- `tools/oracle_from_trace.cpp`
- Boost Graph 1.92.0 source archive SHA-256: `5c1d40cb8e19adbf740a4ec2da35b3e58f3f5804b1dce44deb53df72193cbc6c`
- every query must pass ADES fresh-Dijkstra = Boost.Graph Dijkstra agreement before comparative timing is admitted.

## 2. Frozen non-road topology family

PR80 uses the same **graph family definition** as PR68 but fresh, previously unused PR80 seeds.

Logical topology:

- deterministic Barabási–Albert-style preferential attachment;
- simple connected undirected graph;
- 15,000 vertices;
- 5 attachments per new vertex;
- 74,985 logical undirected edges;
- positive integer initial weights in `[100,1000]`.

ADES/VeloGraphX receive the exact symmetric directed representation: each logical undirected edge `(u,v,w)` becomes adjacent `u→v` and `v→u` DIMACS arcs with identical weight.

Fresh graph seeds, frozen before timing:

- `8209`
- `9103`
- `10037`

These differ from PR68's consumed graph seeds `3109,4001,5011`.

The graph files are generated deterministically inside the evidence workflow. Their SHA-256 identities are recorded before any comparative pair executes and propagated to both implementations.

## 3. Frozen workloads

Fresh workload seeds, frozen before timing:

- `12007`
- `13001`

These differ from PR68's consumed workload seeds `6101,7103` and PR66's road holdout seeds.

Four frozen arbitrary-source families per graph/workload-seed pair:

1. `uniform`
   - 60 distinct sources, exactly zero source reuse.
2. `hot-pool-2`
   - sources drawn from a fixed pool of 2;
   - required source-reuse fraction ≥ 0.80.
3. `hot-pool-4`
   - sources drawn from a fixed pool of 4;
   - required source-reuse fraction ≥ 0.80.
4. `rotating-hot-4`
   - disjoint four-source hot pools rotate every 30 queries;
   - required source-reuse fraction ≥ 0.80.

Per cell:

- 60 queries;
- one logical weight replacement every 5 queries;
- 12 logical weight replacements;
- logical replacements alternate decrease/increase strictly;
- deltas sampled deterministically from `[5,50]`;
- ADES and VeloGraphX execute the same two directed arc updates per logical undirected replacement before the next query;
- query exactness is checked only after a complete logical paired update;
- no future knowledge is permitted.

Total preregistered cells:

`3 graph seeds × 4 source families × 2 workload seeds = 24 cells`.

## 4. Equal logical-state contract

Persistent algorithmic state budget for **both** implementations:

`67,108,864 bytes (64 MiB)`.

Hard gates inherited from PR43/PR54/PR65:

- same graph SHA-256;
- same trace SHA-256;
- same operation/query/update counts;
- exact answers;
- online arrival order;
- no future knowledge;
- `algorithm_ns = query_ns + update_ns`;
- accounted persistent algorithm state ≤ 64 MiB;
- VeloGraphX retained-source state accounting present and positive;
- ADES accounting version 2;
- VeloGraphX adapter accounting version 2;
- process peak RSS reported separately and never substituted for the logical-state contract.

## 5. Execution order and timing

Each cell has two measured matched repetitions:

- repetition 0: VeloGraphX → ADES
- repetition 1: ADES → VeloGraphX

Primary cell ratio:

`VeloGraphX algorithm_ns / ADES algorithm_ns`

Primary sensitivity ratio:

`VeloGraphX query_ns / ADES query_ns`

Update-only ratio is reported diagnostically but is not required to pass because the systems have different update/query decomposition strategies.

Initialization/build/input generation/oracle construction are excluded from primary algorithm time and retained as provenance. The exact per-run binaries, graph/trace identities, logical state, cache counters, and RSS are retained.

## 6. Statistical rule

For each cell, the two order-balanced repetition ratios are geometric-meaned by the frozen PR65 pair driver.

Across cells, inference is over log cell-level ratios with a two-sided 95% t interval, matching the PR66 holdout methodology.

Report:

- aggregate total ratio and 95% CI;
- aggregate query-only ratio and 95% CI;
- aggregate update ratio and 95% CI;
- each source-family total/query ratio and CI;
- each graph-instance total/query ratio and CI;
- all individual cells;
- all total/query loss cells;
- VeloGraphX cache hits/misses/evictions;
- accounted state and process RSS.

The interval characterizes this preregistered 24-cell slice; it is not population-level inference over all graph families.

## 7. Strong support rule

PR80 supports the preferred cross-product hypothesis **only if all conditions hold**:

1. exactness passes in 24/24 cells;
2. independent oracle agreement passes in 24/24 cells;
3. all trace, graph, online-order, no-future-knowledge, and 64 MiB logical-state gates pass;
4. aggregate total lower 95% CI > 1;
5. aggregate query-only lower 95% CI > 1;
6. every one of the four source-family total lower 95% CIs > 1;
7. every one of the four source-family query-only lower 95% CIs > 1;
8. every one of the three graph-instance total lower 95% CIs > 1;
9. every one of the three graph-instance query-only lower 95% CIs > 1;
10. there are **zero** individual total-time loss cells;
11. there are **zero** individual query-time loss cells.

Support disposition:

`SUPPORTS_ADES_OVER_VELOGRAPHX_ON_PR80_NONROAD_ARBITRARY_SOURCE_SLICE`

Any completed, semantically valid experiment that fails one or more performance conditions receives:

`FALSIFIES_STRONG_PR80_VELOGRAPHX_NONROAD_HYPOTHESIS_ON_TESTED_SLICE`

Any correctness, identity, accounting, or oracle failure receives:

`REJECTED_PR80_INTEGRITY_OR_EXACTNESS_GATE`

CI must accept either completed performance disposition. A valid falsification is not a CI failure.

## 8. Claim boundary

Even a strong pass permits only the following practical statement:

**Under the frozen exact-online arbitrary-source contract and equal 64 MiB logical-state budget, the frozen ADES-V3 candidate outperformed the pinned, unmodified-upstream VeloGraphX comparator with its qualified byte-bounded source adapter across all 24 preregistered cells on three fresh deterministic non-road preferential-attachment graph instances, including uniform and high source-reuse regimes. Combined with the independent PR66 NY-road holdout, this supplies cross-topology evidence against the same dynamic-maintenance comparator.**

It does **not** establish:

- theoretical SOTA;
- universal practical SOTA;
- superiority over every dynamic shortest-path implementation;
- population-level performance over arbitrary graph distributions;
- GPU/distributed superiority;
- equal process RSS;
- native arbitrary-source VeloGraphX semantics without the transparent adapter.

If PR80 strongly supports the hypothesis, the next R7 step is evidence synthesis and comparator-registry/publication-contract reconciliation before any broader SOTA wording. If PR80 falsifies the hypothesis, the losing cells become the next optimization target; no post-hoc seed, family, or threshold changes are permitted.