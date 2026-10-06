# ADES — Adaptive Dynamic Exact Shortest Paths

ADES is a research/engineering prototype for **exact** shortest-path queries on directed graphs with nonnegative integer weights under online edge-weight increases and decreases.

**Evidence status:** reconstruction/reproducibility phase. Novelty is **not established** and ADES is **not claimed to be universally fastest**.

## Current repository slice

This first clean slice deliberately prioritizes correctness over speculative optimization:

- stable edge IDs with forward/reverse adjacency;
- overflow-safe exact Dijkstra oracle;
- exact bidirectional Dijkstra for cold point-to-point queries;
- cold → probation → resident source lifecycle;
- conservative update filtering using shortest-path tightness;
- safe full-SSSP fallback whenever a resident state may be invalidated;
- deterministic differential/regression tests;
- DIMACS `.gr.gz` loader and CLI.

The handoff's local affected-region repair and self-calibrating repair/rebuild controller are the next implementation slice. They are not silently represented by the conservative fallback.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires C++20, CMake, and zlib.

## Dataset

The development benchmark uses the 9th DIMACS Shortest Paths NY road graphs (`USA-road-d.NY.gr.gz`, `USA-road-t.NY.gr.gz`) and coordinates. Large datasets are intentionally not committed.

## Research discipline

Refute first. Measure second. Prove where possible. Claim only what survives.
