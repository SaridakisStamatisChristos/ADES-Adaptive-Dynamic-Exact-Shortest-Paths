# NY DIMACS Evidence Provenance

This directory permanently archives the recovered ADES NY DIMACS evidence package generated on 2026-10-07.

## Source runs

- Base definitive evidence run: GitHub Actions run `37597808415`.
- Recovery run: GitHub Actions run `37605773214`.
- Successful cross-run aggregation: GitHub Actions run `37610905480`.
- Aggregated artifact ID: `11477407539`.
- Aggregated artifact ZIP SHA-256 reported by GitHub Actions: `40c35aabdbf27e31b14c5049356c891003ce190cddb1063f49cf835e8b7a813e`.

The recovery run replaced the previously failing `USA-road-t.NY-local` evidence and supplied a fresh time-controller result after the SpatialGrid initialization undefined-behavior fix. The remaining evidence originates from the successful cells of the base run.

## Relevant commits

- Base evidence trigger commit: `25ed24ae9955b55e3c3dbe2a209ef7d43fb9c4e3`.
- SpatialGrid UB fix merged to main: `edcd145e12f5e43bd08c6eea31b1efe4a3066686`.
- Recovery evidence trigger commit: `415b73bb9ff324c85e880e78d3f1517178028b1c`.
- Successful aggregation trigger: `b2ef9fffb4ad325f13f57ab53b5702deca99cd7a`.
- Summarizer/CI repair merged to main: `d92a2941cbe680d5a25faefd8ee5fcc18022edc1`.

## Interpretation boundary

This package is valid engineering evidence and preserves the exact recovered benchmark outputs. It is intentionally labeled as mixed-provenance evidence: most base matrix cells were produced before the SpatialGrid initialization bug was repaired, although those cells completed successfully. For publication-grade homogeneous evidence, rerun the complete matrix on one fixed commit with multiple independent seeds/traces and a predeclared statistical protocol.

The five repetitions in each matrix cell are timing repetitions of the same deterministic trace for the configured seed, not five independent workload samples. `p95` values in `SUMMARY.md` are descriptive statistics, not confidence intervals.

## Integrity

`SHA256SUMS` records SHA-256 digests for every archived evidence file and this provenance document. The raw CSVs are the primary evidence; `SUMMARY.md` is derived from them by `benchmarks/summarize_results.py`.
