# PR49 Publication Viability Pilot Evidence

This directory permanently preserves the PR49 pilot produced by GitHub Actions run `37887233906` from source commit `1c3813391a041cf2a4ef55076f20478a1eb7dcd2`.

The frozen pilot decision is **REFRAME**. This is a scientific pilot outcome, not a scoped-SOTA claim. The PR48 DIRECT external-comparator set was empty at execution time, so the frozen PR49 decision rule did not permit `GO`.

Browsable files:

- `ANALYSIS.md` — generated interpretation under the predeclared decision rule.
- `decision.json` — machine-readable GO / REFRAME / NO-GO result.
- `manifest.json` — frozen matrix, source commit, statistics, and provenance.
- `SHA256SUMS` — hashes for this permanent repository package.

`compact-evidence.tgz` contains the complete compact scientific package: `measurements.csv`, `summary.csv`, `confidence.csv`, the analysis/decision/manifest files, every canonical Workload Generator v2 trace and trace metadata file, and every exact trace-specific oracle bundle.

The original Actions artifact also contains per-process stdout/stderr/RSS files and regenerated PR47 synthetic graphs. The graphs are deterministic and already pinned by the dataset registry; per-process numeric RSS/timing values are retained in `measurements.csv`.
