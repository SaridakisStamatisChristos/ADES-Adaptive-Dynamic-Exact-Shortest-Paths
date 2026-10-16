# PR90 — Submission-readiness adversarial audit

## Mission

PR90 asks a different question from PR81–PR89:

> Assuming the scientific evidence and conservative claim boundaries are correct, is the paper packaged well enough that a technically serious reviewer can read, render, inspect, and falsify it without hidden provenance gaps?

This is not an acceptance prediction. A venue may reasonably reject ADES on novelty, comparator breadth, scope, or perceived systems significance even if this audit passes.

## Inputs frozen before PR90

- frozen ADES candidate: `cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`;
- PR87 publication manifest and claim package;
- PR88 independent raw-artifact recomputation, run `38865585699`, retained artifact `12030529254`, digest `sha256:edb03af36c4c6188866c0ec10a86c2394ea95ea8d185e93ee375a224534b1ee5`;
- PR89 citation-complete manuscript, head `a780859668dbdad3cd9efd3ca0ea77ded5775b7d`;
- PR89 retained submission source artifact `12031118435`, digest `sha256:c3fe768e5b7748b69bc4f8ad4779ca4deb881c47b2c807fe20348c50a0ed484f`.

PR90 is forbidden from altering solver, comparator, workload, benchmark-evidence, publication-contract, novelty-ledger, or historical-scaffold surfaces.

## Adversarial findings

### R1 — Experimental machine disclosure was insufficiently visible in PR89

**Classification:** repairable publication-packaging issue; not a benchmark invalidation.

The canonical artifacts already retained environment records, but PR89 did not put the central machine provenance into the reviewer-facing paper package. PR90 adds `paper/EXPERIMENT_ENVIRONMENT.md` from those archived records.

The disclosure matters because the central campaigns do not all use the same reported CPU model:

- PR62: 4 logical CPUs, AMD EPYC 9V45 96-Core Processor;
- PR66: 4 logical CPUs, AMD EPYC 7763 64-Core Processor;
- PR80: 4 logical CPUs, AMD EPYC 7763 64-Core Processor.

PR80 additionally records Ubuntu 24.04 hosted-runner lineage, Python 3.12.3, GCC 13.3.0, and CMake 3.31.6.

**Mandatory interpretation:** central ratios are matched within experiment cells/runs. Absolute timings or ratio magnitudes across PR62/PR66/PR80 must not be narrated as if they came from one identical physical host.

### R2 — Hosted virtualized timing remains a limitation

**Classification:** declared external-validity / performance-measurement limitation.

The benchmark evidence was produced on GitHub-hosted Azure VMs rather than dedicated bare metal. Matched comparisons, execution-order controls, repeated cells, exactness gates, per-cell ratios, loss-cell reporting, and preregistration reduce several threats, but they do not eliminate cloud scheduling and frequency variation.

A stronger future validation would rerun the frozen experiment contracts on separately administered dedicated hardware.

### R3 — Comparator breadth remains limited

**Classification:** material reviewer risk; not repaired by wording.

The publication-grade DIRECT set has two external comparators:

1. Boost.Graph exact recomputation;
2. pinned VeloGraphX through the qualified arbitrary-source adapter.

GraphBolt and NetworKit evidence remains contextual/assumption-separated and is not silently promoted. A reviewer may judge two DIRECT external baselines insufficient for a broad systems-SOTA claim. This is one reason the paper retains a scoped rather than universal SOTA conclusion.

### R4 — VeloGraphX arbitrary-source semantics are adapter-mediated

**Classification:** mandatory semantic boundary.

Pinned upstream VeloGraphX is fixed-source and unmodified. The arbitrary-source workload is implemented by a transparent, byte-bounded online LRU of independent upstream source instances. Its time and retained state are charged. The manuscript must never collapse that into “native upstream arbitrary-source VeloGraphX.”

### R5 — Memory fairness is a logical-state contract, not equal RSS

**Classification:** mandatory resource-model boundary.

The common `67,108,864`-byte allowance is a maximum algorithm-owned persistent-state cap. It does not require equal actual occupancy and does not establish equal process RSS. PR80 in fact records substantially different accounted peaks for ADES and VeloGraphX.

### R6 — Statistical intervals are slice-level, not population-level

**Classification:** mandatory inference boundary.

The log-space 95% t intervals summarize the declared 24-cell experimental slices. They do not justify population-level confidence statements over arbitrary road or non-road graph distributions.

### R7 — PR62 cannot carry the source-breadth claim

**Classification:** corrected interpretation that must remain visible.

All executed PR62 workloads have `locality_percent=0`. PR62 supports the external-recomputation result only. PR66 is the experiment that genuinely exercises source reuse against VeloGraphX.

### R8 — Broad algorithmic novelty is not available as a submission claim

**Classification:** `NO_GO`, unchanged.

PR86 found substantial prior art for dynamic SSSP repair, predecessor/non-predecessor filtering, alternative/replacement witnesses, shortest-path caching, admission/eviction economics, maintenance-aware materialization, and cost-based incremental-versus-static control.

The manuscript may position the integrated exact system and evidence discipline as a contribution, but **broad algorithmic novelty remains rejected** and integrated-system originality remains unestablished/possibly distinct.

### R9 — PR88 is not third-party replication

**Classification:** provenance-strength boundary.

PR88 independently re-downloads pinned raw artifacts, verifies identities, and recomputes headline statistics from per-cell files without trusting archived aggregate summaries. It is strong artifact-level reproducibility evidence. It is not an external laboratory rerun on independently administered hardware.

### R10 — Fixed topology remains part of the empirical contract

**Classification:** scope limitation.

The publication experiments exercise mixed edge-weight increases/decreases on fixed graph topology. Literature may support broader dynamic operations, but the empirical conclusion may not be generalized to arbitrary edge insertion/deletion streams without new evidence.

## PR90 repairs

PR90 adds only reviewer-facing publication surfaces:

1. `paper/EXPERIMENT_ENVIRONMENT.md` — archived machine provenance and hardware-variation interpretation;
2. `paper/ARTIFACT_README.md` — reviewer recomputation and falsification quick-start;
3. `paper/submission.css` — deterministic print-safe venue-neutral rendering rules;
4. this audit;
5. `.github/workflows/pr90-submission-readiness.yml` — executable render, citation, claim-boundary, PDF-text, and publication-only-diff gate.

The PR90 workflow renders `paper/ADES_MANUSCRIPT.md` plus the environment appendix with Pandoc citation processing, converts the standalone HTML to PDF, extracts PDF text, rejects unresolved citation markers, checks central evidence and limitations, and retains the rendered package as an Actions artifact.

## Acceptance condition

PR90 may return `SUBMISSION_READY_WITH_DECLARED_LIMITATIONS` only if all of the following are true:

- PR89 manuscript and bibliography remain present and citation-resolvable;
- PR88/PR89 provenance pins remain unchanged;
- the rendered PDF is nonempty and extractable;
- no Pandoc citation syntax such as `[@key]` survives into rendered output;
- all six central rounded performance values appear in rendered text;
- adapter, state-cap, PR62, statistics, novelty, PR88, and hardware-variation boundaries remain visible;
- the environment appendix exposes both EPYC 9V45 and EPYC 7763 campaigns;
- the artifact quick-start preserves the pinned run/artifact/hash identities;
- PR90 changes only the five declared publication surfaces;
- independent repository correctness/comparator gates remain green.

## Disposition before execution

**PENDING_EXECUTABLE_GATE.**

A passing PR90 gate supports the narrower operational statement:

> The ADES manuscript and artifact package are technically submission-ready, with material scientific and external-validity limitations explicitly declared.

It does not predict peer-review acceptance and does not strengthen the scientific claim beyond PR81/PR86.
