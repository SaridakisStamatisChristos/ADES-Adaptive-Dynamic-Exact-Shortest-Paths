# Dataset registry

`registry.json` is the canonical publication dataset inventory introduced by PR47.

It separates:

- **topology identity** from weight-metric variants;
- **committed external data** from deterministic generated data;
- **scientific qualification** from redistribution/license review;
- human-readable names from cryptographic file identity.

To materialize and validate the complete registry:

```bash
python3 tools/generate_synthetic_graphs.py benchmarks/generated
python3 tools/validate_dataset_registry.py \
  --registry benchmarks/datasets/registry.json \
  --generated-dir benchmarks/generated
```

A publication campaign making a general-graph claim must use at least five distinct registered topology classes and at least one non-road class. Two weight functions on the same topology do not satisfy that diversity rule.

See `docs/MULTIGRAPH_DATASET_REGISTRY_PR47.md` for the frozen PR47 contract.
