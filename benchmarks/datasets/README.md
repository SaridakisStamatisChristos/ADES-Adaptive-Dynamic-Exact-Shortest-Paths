# Dataset registry

`registry.json` is the canonical publication dataset inventory introduced by PR47 and extended to schema v2 by PR75.

It separates:

- **topology identity** from weight-metric variants;
- **committed external data**, **evidence-qualified remote external data**, and deterministic generated data;
- **scientific qualification** from redistribution/license review;
- local byte identity from externally qualified raw/canonical topology identity;
- human-readable names from cryptographic identity and canonical evidence references.

## Dataset kinds

### `generated`

Deterministic repository-generated DIMACS graphs. The registry validator materializes them with `tools/generate_synthetic_graphs.py`, verifies their SHA-256 values and DIMACS counts, and the dataset-registry workflow smoke-loads them through Workload Generator v2.

### `external-committed`

External datasets whose bytes are committed in the repository. The validator checks the committed bytes, SHA-256, and DIMACS counts locally.

### `external-qualified-remote`

External third-party topologies whose raw bytes are **not** committed merely to satisfy the registry. These entries are admitted only after a dedicated qualification PR has frozen:

- the exact upstream artifact URL;
- raw source-artifact SHA-256;
- canonical-topology SHA-256;
- vertex/edge and representation statistics;
- parser/canonicalization semantics;
- canonical GitHub PR, head SHA, workflow run, artifact ID, and artifact ZIP SHA-256.

The ordinary registry validator checks those immutable evidence references and metadata offline. It does **not** claim to have redownloaded remote bytes during every registry run. Fresh-download reproducibility belongs to the dedicated qualification gate recorded by the entry.

Remote third-party entries must keep `redistribution_review_required: true` unless a separate review establishes redistribution rights. PR75 intentionally does not infer dataset redistribution rights from the BSD license of the SNAP software library.

## Validation

To materialize and validate the local portion of the complete registry:

```bash
python3 tools/generate_synthetic_graphs.py benchmarks/generated
python3 tools/validate_dataset_registry.py \
  --registry benchmarks/datasets/registry.json \
  --generated-dir benchmarks/generated
```

Schema v2 requires at least five distinct registered topology classes and at least two distinct **external real topology classes**. Metric variants on one topology still count only once. This inventory/evidence rule does not by itself establish a general-graph performance claim: benchmark-specific correctness, comparator, workload, and statistical evidence remain separate requirements.

Current evidence-qualified remote topologies include:

- SNAP `email-Eu-core` — directed email/communication topology, qualified by PR71; fixed-source GraphBolt performance evidence in PR72 uses deterministic synthetic positive weights and does not claim native email weights.
- SNAP `p2p-Gnutella08` — directed peer-to-peer topology, qualified by PR73; fixed-source GraphBolt performance evidence in PR74 uses deterministic synthetic positive weights and does not claim native Gnutella weights.

See `docs/MULTIGRAPH_DATASET_REGISTRY_PR47.md` for the original PR47 contract and the evidence references embedded in `registry.json` for later externally qualified topologies.
