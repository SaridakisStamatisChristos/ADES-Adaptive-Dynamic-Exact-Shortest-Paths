# ADES — Adaptive Dynamic Exact Shortest Paths

ADES is a research/engineering prototype for **exact** shortest-path queries on directed graphs with nonnegative integer weights under online edge-weight increases and decreases.

**Evidence status:** implementation and engineering validation are active; novelty is **not established** and ADES is **not claimed to be universally fastest**.

## Formal algorithm specification

The implemented algorithm is specified independently of the C++ source in [`docs/ALGORITHM.md`](docs/ALGORITHM.md). That document defines the online problem, resident/cold state model, pseudocode for query/update/decrease/increase repair, invariants, correctness lemmas and theorem-level proof sketch, adaptive-policy separation, output-sensitive complexity, and current controller limitations.

## Implemented architecture

- stable edge IDs with forward/reverse adjacency and overflow-safe distances;
- exact Dijkstra oracle and exact bidirectional Dijkstra cold queries;
- resident exact SSSP state with selected parent edges;
- strict-improvement decrease propagation;
- increase filtering by old tightness;
- selected-parent affected-subtree discovery and boundary-seeded restricted repair;
- conservative exact rebuild for tight non-parent increases;
- read-only semantic discovery before mutation, with exact rebuild fallback on abort;
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
bash tools/fetch_ny_dimacs.sh benchmarks/data
bash tools/verify_ny_dimacs.sh benchmarks/data
```

The script downloads the canonical Challenge 9 files and rejects any checksum mismatch. The dedicated `NY DIMACS validation` GitHub Actions workflow performs the same acquisition/verification before build, tests, and distance/time smoke queries.

## Current evidence boundary

The repository passes deterministic mixed differential testing against fresh Dijkstra after dynamic repair, adaptive repair control, and cost-aware admission were enabled. The supplied NY distance and travel-time graphs both load as 264,346 vertices / 733,846 arcs; the pinned smoke pair 1→1000 returns 28,939 and 61,253 respectively.

The permanent 2026-10-07 NY evidence package is archived in [`evidence/ny-2026-10-07/`](evidence/ny-2026-10-07/) with raw CSVs, per-workload metadata, generated summary, provenance, and integrity hashes.

That package is engineering evidence, not a novelty or asymptotic-superiority claim. Its provenance document records that the archived matrix combines successful base-run cells with a post-fix recovery cell; publication-grade evidence should rerun the complete matrix on one fixed commit across multiple independent workload seeds and longer traces.

The current controller evidence also refutes a simple structural policy rule: very large affected SPT regions do **not** imply that aborting local repair and rebuilding is cheaper. Future controller work should estimate remaining repair cost against rebuild cost directly.

## Research discipline

**Refute first. Measure second. Prove where possible. Claim only what survives.**
