# NY crossover full-sweep — one-query control (2026-10-08)

**Classification:** completed diagnostic control; **not** a publication-quality performance crossover experiment.

- Repository: `SaridakisStamatisChristos/ADES-Adaptive-Dynamic-Exact-Shortest-Paths`
- Workflow run: https://github.com/SaridakisStamatisChristos/ADES-Adaptive-Dynamic-Exact-Shortest-Paths/actions/runs/37742372168
- Job: https://github.com/SaridakisStamatisChristos/ADES-Adaptive-Dynamic-Exact-Shortest-Paths/actions/runs/37742372168/job/113195813200
- Source commit: `466d85c869d54f5e02c53b542d6b024de9e37749`
- Graph: `benchmarks/data/USA-road-d.NY.gr.gz` (NY DIMACS verification passed)
- Invocation: `bash benchmarks/run_phase_diagram.sh benchmarks/data/USA-road-d.NY.gr.gz results/crossover/phase.csv 1`
- Design: 5 seeds × 6 query families × 5 update intervals × 6 hot-source settings × 4 baselines = **3,600 cells**.
- Benchmark step: 2026-10-08 07:16:20 UTC to 07:39:13 UTC (approximately 22m53s).
- Outcome: all workflow steps passed; evidence artifact uploaded.
- Artifact ID: `11535725192`
- Artifact name: `ades-crossover-evidence-466d85c869d54f5e02c53b542d6b024de9e37749`
- Artifact SHA-256 (GitHub Actions digest): `55e494292abfcfda1887e65ab07828d3a52c5897fa19af9a9a32d7c1be89a2ad`
- Artifact contents: `phase.csv` (3,600 data records), `phase-summary.csv`, `SUMMARY.md`.
- GitHub artifact retention: 90 days.
- Independent preserved ZIP copy: ChatGPT Library `/ADES/crossover-evidence/2026-10-08-run-37742372168/ADES-crossover-1query-control.zip`.

## Interpretation and limitations

This run establishes that the **complete sweep can terminate successfully with one query per cell**. It does **not** establish realistic-query crossover behavior. Earlier runner shutdown causes remain unproven.

The archived evidence is **not committed into the Git repository** by this provenance-only change. The canonical raw evidence remains the GitHub Actions artifact, with a separately preserved ZIP in the user's Library. Do not describe this manifest as equivalent to versioning the CSV data in Git.

## Follow-up

Run bounded diagnostic experiments for 1, 10, and 100 cells with explicit per-cell progress and memory metrics; assess scaling before attempting a larger full sweep.
