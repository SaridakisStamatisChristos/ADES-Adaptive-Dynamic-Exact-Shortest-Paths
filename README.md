# ADES — Adaptive Dynamic Exact Shortest Paths

ADES is a research/engineering prototype for **exact** shortest-path queries on directed graphs with nonnegative integer weights under online edge-weight increases and decreases.

**Evidence status:** reconstruction/reproducibility phase. Novelty is **not established** and ADES is **not claimed to be universally fastest**.

## Implemented architecture

- stable edge IDs with forward/reverse adjacency and overflow-safe distances;
- exact Dijkstra oracle and exact bidirectional Dijkstra cold queries;
- resident exact SSSP state with selected parent edges;
- strict-improvement decrease propagation;
- increase filtering by old tightness;
- selected-parent affected-subtree discovery and boundary-seeded restricted repair;
- conservative exact rebuild for tight non-parent increases;
- read-only discovery before mutation, with rebuild fallback on abort;
- per-resident EWMA repair/rebuild controller whose repair budget adapts from measured cost;
- cost-aware source admission using accumulated cold-search work rather than recurrence alone;
- deterministic regression/adversarial tests and mixed differential testing against fresh Dijkstra.

The adaptive controller affects performance policy only. Exactness is protected by filtering rules, local-repair invariants, and full-SSSP fallback.

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
bash tools/fetch_ny_dimacs.sh benchmarks/data\nbash tools/verify_ny_dimacs.sh benchmarks/data
```

The script downloads the canonical Challenge 9 files and rejects any checksum mismatch. The dedicated `NY DIMACS validation` GitHub Actions workflow performs the same acquisition/verification before build, tests, and distance/time smoke queries.

## Current evidence boundary

Local reconstruction currently passes the deterministic mixed differential suite (12 seeds × 12,000 operations) after dynamic repair, adaptive repair control, and cost-aware admission were enabled. The supplied NY distance and travel-time graphs both load as 264,346 vertices / 733,846 arcs; the current smoke pair 1→1000 returns 28,939 and 61,253 respectively.

These are engineering-validation results, not novelty or asymptotic-superiority claims. Full workload benchmarking, controller ablations, memory accounting, complexity documentation, and fresh literature preflight remain required.

## Research discipline

**Refute first. Measure second. Prove where possible. Claim only what survives.**
