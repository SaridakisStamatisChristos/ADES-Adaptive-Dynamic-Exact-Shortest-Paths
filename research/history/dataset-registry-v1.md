# PR47 — Multi-Graph Dataset Registry

## Purpose

PR47 prevents publication claims from depending implicitly on one road-network topology. It introduces a machine-verifiable dataset registry with cryptographic identity, provenance, structural class, parser qualification, and license metadata.

**Claims addressed:** `C2`, `C4`, `C8`, and the scope discipline required before `C-SOTA` can be evaluated.

## Generality rule

A publication-facing campaign that makes a claim about general weighted directed graphs must draw from at least **five distinct topology classes** and must include at least one non-road topology.

Different weight functions on the same topology do **not** count as different graphs for this rule. In particular, the NY distance and NY travel-time instances share the single `road` topology class.

## Registered topology classes

PR47 qualifies six classes:

1. `road` — committed 9th DIMACS New York road topology;
2. `grid` — deterministic 224 x 224 bidirectional 2-D grid;
3. `uniform-random` — deterministic sparse directed random topology;
4. `scale-free` — deterministic preferential-attachment topology;
5. `small-world` — deterministic rewired ring-lattice topology;
6. `clustered` — deterministic community-structured directed topology.

The five synthetic non-road instances are approximately 50k vertices each and are generated from source rather than stored as large repository blobs.

## Registry

The canonical registry is:

```text
benchmarks/datasets/registry.json
```

Each dataset entry records:

```text
id
display_name
kind
topology_class
weight_semantics
format
directed_representation
vertices
arcs
local_path
sha256
source or generator provenance
license metadata
qualification metadata
```

The SHA-256 is over the exact input file bytes consumed by the benchmark. For committed `.gr.gz` files this means the compressed file bytes. For generated graphs this means the canonical uncompressed DIMACS text bytes.

## Synthetic graph determinism

`tools/generate_synthetic_graphs.py` is the sole generator for PR47 synthetic datasets.

It uses:

- explicit integer arithmetic;
- a fixed SplitMix64 implementation;
- canonical lexicographic edge ordering;
- deterministic positive edge weights in `[1, 100000]`;
- canonical ASCII DIMACS output;
- no timestamps;
- no platform-dependent compression layer.

The generated `.gr` files remain uncompressed so their identity is independent of gzip-header implementation details. ADES reads them through zlib's transparent input path.

Changing generator semantics requires new dataset IDs and new pinned hashes; an existing registered dataset must never silently change bytes.

## External-data license boundary

The committed NY data is recorded as derived from the U.S. Census TIGER/Line road graph distributed through the 9th DIMACS Implementation Challenge.

PR47 deliberately does **not** invent a definitive license statement for the historical DIMACS packaging. The registry records that exact packaging/redistribution terms are not asserted by this PR and flags the external corpus for redistribution review.

This is separate from scientific-use qualification. The existing NY bytes remain cryptographically pinned and scientifically usable as a benchmark input; any future redistribution package must respect the registry's license-review flag.

The five synthetic datasets contain no third-party data and therefore do not carry an external dataset license dependency.

## Validation

`tools/validate_dataset_registry.py` fails if:

- a required metadata field is absent;
- a dataset ID or local path is duplicated;
- SHA-256 syntax is invalid;
- the materialized file is missing;
- the file hash differs from the registry;
- DIMACS vertex or arc counts differ from the registry;
- an external dataset lacks source provenance;
- a generated dataset lacks generator provenance;
- license metadata is incomplete;
- parser/hash qualification is not explicit;
- fewer than five distinct topology classes are qualified;
- a non-road topology is absent;
- the registry allows metric variants to inflate topology diversity.

## CI acceptance gate

`.github/workflows/dataset-registry.yml`:

1. verifies the committed NY corpus;
2. regenerates all five synthetic graph instances from scratch;
3. validates exact SHA-256 and DIMACS counts for every registered dataset;
4. builds ADES and runs the complete CTest suite;
5. smoke-loads every synthetic graph through `ades_workload_v2`;
6. independently records generated SHA-256 values;
7. uploads the validation summary and smoke metadata.

## Interpretation

The synthetic graph families broaden structural coverage; they are not substitutes for additional real-world datasets.

A later result may support a claim only over the graph classes actually tested. If ADES is competitive only on road-like or locality-rich classes, the final paper must narrow its claim accordingly rather than treating the registry as evidence of universal performance.

Negative, neutral, and ADES-unfavorable graph classes remain admissible evidence and must not be removed post hoc.
