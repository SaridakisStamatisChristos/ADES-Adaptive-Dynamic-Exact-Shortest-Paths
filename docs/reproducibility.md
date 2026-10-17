# Reproducibility

ADES treats reproducibility as part of the research result, not as optional packaging.

## Required identity

A performance result should identify at least:

- source commit SHA;
- compiler, version and relevant flags;
- operating system/kernel and hardware;
- thread count;
- graph identity and checksum;
- workload configuration and seed;
- canonical trace identity;
- ADES/controller configuration;
- comparator identity and qualification status;
- persistent-state budget when applicable;
- warmup/repetition policy;
- exactness status;
- raw measurements and summary method.

A failed exactness, trace-identity, checksum, provenance or comparator-contract check invalidates the corresponding performance result.

## New York DIMACS corpus

Fetch and verify the repository's canonical road-network inputs with:

```bash
bash tools/fetch_ny_dimacs.sh benchmarks/data
bash tools/verify_ny_dimacs.sh benchmarks/data
```

Do not substitute similarly named graph files without updating the explicit experiment protocol and provenance.

## Build and tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The CTest suite covers the main algorithm, trace identity, memory accounting, telemetry, ablation profiles and workload generation. GitHub Actions add dataset, comparator and experiment-specific gates.

## Baseline stability

`B0` through `B4` are stable scientific identifiers. Their semantics are documented in the root README and must not be silently redefined between experiments. Bounded variants used by historical experiments are likewise preserved when required by a frozen protocol.

## Historical experiments

Historical campaign documents and frozen driver implementations are stored under `research/`. Archived evidence remains under `evidence/` and checked-in result packages under `results/`. Older identifiers can include pull-request or workflow-run numbers because those identifiers are part of provenance; they are not current product milestones.

The files inside frozen evidence packages should not be rewritten merely to modernize wording. Their hashes, manifests and original interpretation are part of the audit trail.

## Measured facts versus hypotheses

A report should label measured observations separately from hypotheses about why those observations occurred. Negative and contradictory results are retained. A performance hypothesis that fails a preregistered or otherwise valid test remains useful evidence and must not be deleted to simplify the repository narrative.
