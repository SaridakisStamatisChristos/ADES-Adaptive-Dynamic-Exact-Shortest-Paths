# ADES — Adaptive Dynamic Exact Shortest Paths

ADES is a research/engineering prototype for **exact** shortest-path queries on directed graphs with nonnegative integer weights under online edge-weight increases and decreases.

**Evidence status:** under the repository's frozen Publication Contract v1.0, ADES now has **scoped practical SOTA evidence** for the tested exact-online CPU workloads and qualified comparator set. This is **not** a universal or theoretical SOTA claim, and scholarly novelty of the underlying mechanisms is **not established**.

## Formal algorithm specification

The baseline algorithm is specified independently of the C++ source in [`docs/ALGORITHM.md`](docs/ALGORITHM.md). The PR54 core-engine delta is specified in [`docs/PR54_CORE_ENGINE_PHASE_CHANGE.md`](docs/PR54_CORE_ENGINE_PHASE_CHANGE.md): it tightens one increase filter without weakening exactness, moves non-semantic search/repair scratch out of resident state, packs SPT identifiers, and freezes file-loaded topology into compact forward/reverse CSR.

## Implemented architecture

- stable 32-bit edge IDs with an explicitly reserved sentinel for SPT state;
- fixed-topology DIMACS graphs frozen into compact forward/reverse CSR while edge weights remain online-mutable;
- overflow-safe distances;
- exact Dijkstra oracle and exact bidirectional Dijkstra cold queries;
- reusable epoch-marked bidirectional-search workspace, avoiding full `|V|` distance-array initialization on every cold query;
- resident exact SSSP state with one selected shortest-path witness tree;
- 32-bit parent/child/sibling identifiers and shared nonresident repair-membership scratch;
- strict-improvement decrease propagation;
- increase filtering by old tightness;
- O(1) filtering of tight non-selected-parent increases because the selected SPT remains an unchanged shortest-path witness;
- selected-parent affected-subtree discovery and boundary-seeded restricted repair;
- read-only semantic discovery before mutation, with exact rebuild fallback on abort;
- per-resident EWMA repair/rebuild controller whose repair budget adapts from measured cost;
- cost-aware source admission using accumulated cold-search work rather than recurrence alone;
- deterministic regression/adversarial tests and mixed differential testing against fresh Dijkstra.

The adaptive controller affects performance policy only. Exactness is protected by witness-preserving filters, local-repair invariants, and full-SSSP fallback.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires C++20, CMake, and zlib.

## NY DIMACS reproducibility

The benchmark corpus is the 9th DIMACS Shortest Paths NY road network:

- `USA-road-d.NY.gr.gz`
- `USA-road-t.NY.gr.gz`
- `USA-road-d.NY.co.gz`

The canonical repository location for these files is `benchmarks/data/`. A fresh clone can either place the three files there directly or fetch them using the helper. Exact SHA-256 verification is mandatory. Run:

```bash
bash tools/fetch_ny_dimacs.sh benchmarks/data
bash tools/verify_ny_dimacs.sh benchmarks/data
```

The script downloads the canonical Challenge 9 files and rejects any checksum mismatch. The dedicated `NY DIMACS validation` GitHub Actions workflow performs the same acquisition/verification before build, tests, and distance/time smoke queries.

## Current evidence boundary

The repository passes deterministic mixed differential testing against fresh Dijkstra after dynamic repair, adaptive repair control, and cost-aware admission were enabled. PR54 additionally adds focused adversarial/differential tests for tight non-parent filtering, reusable cold-query scratch, compact resident state, shared repair scratch, and CSR representation equivalence. Equal-byte accounting is versioned so the smaller resident representation is visible rather than silently changing capacity semantics.

The supplied NY distance and travel-time graphs both load as 264,346 vertices / 733,846 arcs; the pinned smoke pair 1→1000 returns 28,939 and 61,253 respectively.

The permanent 2026-10-07 NY evidence package is archived in [`evidence/ny-2026-10-07/`](evidence/ny-2026-10-07/) with raw CSVs, per-workload metadata, generated summary, provenance, and integrity hashes. That package remains useful engineering evidence, including its recorded post-fix recovery cell, but it is no longer the sole basis for publication-facing performance conclusions.

Subsequent publication-oriented campaigns re-earned the central evidence on frozen candidates with independent workload seeds, matched traces, exact independent oracles, explicit comparator qualification, and auditable persistent-state budgets. The current scientific decision is documented in [`docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md`](docs/PR81_SCOPED_PRACTICAL_SOTA_DECISION.md), the VeloGraphX DIRECT-registry reconciliation in [`docs/PR82_VELOGRAPHX_REGISTRY_RECONCILIATION.md`](docs/PR82_VELOGRAPHX_REGISTRY_RECONCILIATION.md), and the claim-hostile audit protocol in [`docs/PR83_ADVERSARIAL_PUBLICATION_AUDIT.md`](docs/PR83_ADVERSARIAL_PUBLICATION_AUDIT.md).

### Scoped practical-SOTA evidence

The frozen claim domain is deliberately narrow: directed nonnegative weighted graphs, online mixed edge-weight increases/decreases, exact arbitrary-source point-to-point queries, no future knowledge, and bounded auditable algorithm-owned persistent state.

Within that scope, the canonical comparator registry now contains two `DIRECT` external implementations:

- **Boost.Graph Dijkstra recomputation** — a pinned exact arbitrary-source cold-recomputation baseline;
- **VeloGraphX weighted SSSP through the qualified arbitrary-source adapter** — pinned upstream VeloGraphX remains fixed-source, while the transparent ADES-side adapter supplies a causal byte-bounded LRU of independent source instances and charges retained graph + SSSP state to the common persistent-state budget.

Fresh matched holdouts include:

- **PR62:** a fresh 24-cell Boost.Graph holdout, with all measured cells exact and no reported total/query loss cells;
- **PR66:** a fresh 24-cell NY-road VeloGraphX source-reuse holdout under a 64 MiB logical-state cap, again with all measured cells exact and no reported total/query loss cells;
- **PR80:** a preregistered 24-cell non-road VeloGraphX holdout spanning three fresh preferential-attachment graph instances, four source-reuse regimes, and two fresh workload seeds. All 24 cells passed exactness and independent-oracle checks. The aggregate VeloGraphX/ADES geometric-mean ratio was approximately **157.68×** for total algorithm time (95% slice-level robustness interval **[95.33×, 260.80×]**) and **30.20×** for query time (**[16.22×, 56.23×]**), with zero individual total/query loss cells.

The PR80 intervals characterize robustness across that preregistered nested benchmark slice; they are **not** population-level inference over all graphs or workload distributions. The 64 MiB fairness rule is a common auditable persistent-state **cap/accounting contract**, not a claim of equal actual memory use or equal process RSS.

Historical-Cilk GraphBolt and later NetworKit results remain assumption-separated/contextual for the complete frozen `C-SOTA` scope. They must not be silently counted as unrestricted DIRECT wins.

The adversarial PR83 audit preserves the following mandatory boundaries:

- use **scoped practical SOTA evidence**, never universal/theoretical SOTA;
- describe VeloGraphX arbitrary-source comparisons as **adapter-mediated**;
- do not equate the common logical-state cap with equal actual bytes or equal RSS;
- do not promote slice-level uncertainty estimates into population-level claims;
- keep assumption-mismatched comparator evidence segregated;
- do not infer scholarly novelty or asymptotic improvement from empirical performance evidence.

Accordingly, wording such as “world's fastest dynamic shortest-path algorithm,” “beats all state-of-the-art shortest-path algorithms,” or “establishes theoretical SOTA” is not supported by this repository's evidence.

The controller evidence also refutes a simple structural policy rule: very large affected SPT regions do **not** imply that aborting local repair and rebuilding is cheaper. Future controller work should estimate remaining repair cost against rebuild cost directly.

## Research discipline

**Refute first. Measure second. Prove where possible. Claim only what survives.**
