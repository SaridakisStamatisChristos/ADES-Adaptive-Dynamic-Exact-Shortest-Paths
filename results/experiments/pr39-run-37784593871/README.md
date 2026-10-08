# PR39 dynamic crossover — permanent evidence archive

This directory permanently preserves the canonical evidence from successful GitHub Actions dynamic crossover run **37784593871**, produced from `main` commit `77a92c49718e412f25fea2ca694ac163d670bcbd`.

## Provenance

- Workflow run: `37784593871`
- Workflow job: `compare` / job `113335982884`
- GitHub Actions artifact ID: `11553939455`
- Artifact name: `ades-dynamic-crossover-37784593871-1`
- Original artifact ZIP SHA-256: `d75e6397b766d70693979e03c539a1db11d5d3927dd56c2c5bfb676e055bc192`
- Artifact size reported by GitHub: `110225` bytes
- Source commit: `77a92c49718e412f25fea2ca694ac163d670bcbd`
- Graph SHA-256: `7b2446c7ffe6179efbc42af6812448e8cb782f12968ca2bb1d50d979343056d4`

## Canonical preserved evidence

- `manifest.json` — exact run configuration and environment manifest.
- `measurements.csv` — all 144 recorded baseline executions.
- `summary.csv` — generated matched-pair descriptive summary.
- `ANALYSIS.md` — generated descriptive analysis from the benchmark workflow.
- `SHA256SUMS` — integrity hashes for the preserved files plus the original Actions artifact digest.

The Actions artifact also contained 432 per-invocation `.stdout`, `.stderr`, and `.time` files. Those are redundant with the canonical tables for the retained result set and are not duplicated individually in Git history; the original artifact identity and cryptographic digest are recorded here for provenance.

## Scope

This run is a matched resident-source-cap dynamic comparison, **not** an equal-byte-memory experiment. It used 100 queries per trace, two repetitions, capacities 2 and 8, seeds 7 and 17, uniform/single-hot/hot-pool source families, update intervals 5 and 10, and alternating genuine weight increases/decreases. Exact query answers were checked against the independent Dijkstra oracle by the benchmark runner.

The generated analysis is descriptive evidence only; it does not by itself establish universal superiority or a state-of-the-art claim.
