# NY DIMACS benchmark data

Place the three compressed 9th DIMACS NY files in this directory **without renaming them**:

- USA-road-d.NY.gr.gz
- USA-road-t.NY.gr.gz
- USA-road-d.NY.co.gz

After placement, verify them from the repository root:

```bash
sha256sum -c benchmarks/data/NY.sha256
```

Then build and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
bash benchmarks/ny_smoke.sh ./build/ades_cli benchmarks/data
./build/ades_bench benchmarks/data/USA-road-d.NY.gr.gz 200 7 3
./build/ades_bench benchmarks/data/USA-road-t.NY.gr.gz 200 7 3
```

## Repository policy

The dataset files may be committed here when redistribution terms permit. Until they are committed, this folder remains the canonical placement path for manually supplied copies. The checksum manifest pins the exact corpus used by ADES.

For convenience, `tools/fetch_ny_dimacs.sh benchmarks/data` can attempt to acquire the canonical corpus automatically. A clone must never silently benchmark a different dataset: checksum verification is mandatory.

## CI validation

Changes under `benchmarks/data/` trigger the NY DIMACS validation workflow, which verifies the pinned corpus, builds and tests ADES, checks the pinned smoke distances, and executes the B0–B4 NY exactness gate.

<!-- NY CI diagnostic trigger: validates committed corpus and full benchmark gate. -->
