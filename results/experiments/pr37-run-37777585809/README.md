# PR37 benchmark evidence — GitHub Actions run 37777585809

This directory permanently preserves the canonical evidence from the first successful matched-cap memory crossover diagnostic produced by PR #37.

## Provenance

- Workflow run: `37777585809`
- Job: `113312228970`
- Artifact ID: `11550671278`
- Artifact name: `ades-matched-cap-37777585809-1`
- Source commit: `c7f42aeb78e123f0a2cb161e7881896cc5d305c8`
- Graph: `USA-road-d.NY.gr.gz`
- Graph SHA-256: `7b2446c7ffe6179efbc42af6812448e8cb782f12968ca2bb1d50d979343056d4`
- Execution date: 2026-10-08 UTC
- Result: 96/96 benchmark invocations exited successfully; repository build/tests and NY corpus verification passed.
- Original Actions artifact SHA-256: `fc0f577d6c064dc946ec8d4c46b3c4d5e339c565d3fafcbf3a8adb54b9d20a6e`

## Permanently committed evidence

- `measurements.csv` — canonical 96-row measurement table emitted by the benchmark harness.
- `manifest.json` — exact run parameters, platform, graph hash and evidence classification.
- `ANALYSIS.md` — descriptive analysis of this run only.
- `SHA256SUMS` — hashes of the two original canonical files copied from the Actions artifact.

The original Actions artifact contained 290 files, including per-invocation stdout, stderr and GNU-time files. Those raw per-invocation files are not duplicated here; the artifact identity and SHA-256 above preserve provenance. The canonical measurement table and run manifest are committed to Git history and do not depend on Actions retention.

## Scope

This run is a **matched resident-source-cap diagnostic**, not an equal-byte-memory experiment and not publication-grade evidence by itself. It used 10 queries per invocation, so the `update_every=10` configuration did not actually execute an update: the trace generator inserts updates only after a completed query interval. Therefore this run evaluates source-admission/cache behavior and memory footprint, but it does **not** establish dynamic-update superiority.
