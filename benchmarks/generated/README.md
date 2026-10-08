# Generated benchmark graphs

This directory is the canonical local materialization path for the deterministic non-road graph instances registered in `benchmarks/datasets/registry.json`.

Generate them from the repository root with:

```bash
python3 tools/generate_synthetic_graphs.py benchmarks/generated
```

Then validate every registered graph, including the committed NY DIMACS corpus:

```bash
python3 tools/validate_dataset_registry.py \
  --generated-dir benchmarks/generated
```

The generated `.gr` files are intentionally not committed. Their exact canonical bytes are pinned by SHA-256 in the dataset registry and are regenerated in CI. This avoids repository bloat while retaining byte-level reproducibility.

Do not hand-edit a generated graph. Any generator-semantic change requires a generator-version bump, new dataset IDs, new hashes, and an explicit registry update.
