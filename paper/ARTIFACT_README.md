# ADES Submission Artifact Quick Start

This guide is for reviewers who want to inspect or recompute the evidence behind the manuscript **without accepting the manuscript tables as authority**.

## 1. Scientific scope

The artifact supports the repository's bounded conclusion: scoped practical runtime-memory evidence for the frozen exact-online contract against the qualified DIRECT comparator set. It does not establish universal/theoretical shortest-path SOTA, population-level performance over arbitrary graph distributions, equal process RSS, or broad algorithmic novelty.

The frozen ADES candidate is:

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

VeloGraphX arbitrary-source evaluation is adapter-mediated. Pinned upstream VeloGraphX is fixed-source and unmodified at:

`2aa84ce81a180487239c661b65639e5100136075`

## 2. Canonical central artifacts

| Evidence | Run | Artifact ID | ZIP SHA-256 |
|---|---:|---:|---|
| PR62 Boost.Graph New York | `38711826306` | `11961213712` | `0bd64d43ba48fda4c9e0752ea6acafc1c9fa8ddb7eec2f2fdabc88d1574029ff` |
| PR66 VeloGraphX New York source reuse | `38730045262` | `11971060916` | `c4a3d64af948afd03daa1e983ee2d75eadf838ca94bdc59103c25ff5575dd299` |
| PR80 VeloGraphX non-road holdout | `38856829047` | `12027088809` | `617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100` |

PR88 independently re-downloaded those three ZIPs by artifact ID and recomputed the publication statistics from per-cell files. The canonical PR88 run is `38865585699`; its retained reproduction artifact is ID `12030529254`, digest `sha256:edb03af36c4c6188866c0ec10a86c2394ea95ea8d185e93ee375a224534b1ee5`.

## 3. Fast path: independently recompute the headline evidence

From a checkout containing `tools/reproduce_publication_evidence.py`, obtain the three canonical artifact ZIPs above and save them as `pr62.zip`, `pr66.zip`, and `pr80.zip`. Verify the SHA-256 values before parsing.

Then run:

```bash
python3 tools/reproduce_publication_evidence.py \
  --manifest evidence/pr87_publication_manifest.json \
  --pr62 pr62.zip \
  --pr66 pr66.zip \
  --pr80 pr80.zip \
  --output reproduction.json
```

The verifier deliberately ignores the archived top-level aggregate verdicts. It reads the raw per-cell CSVs and independently recomputes:

- exact-cell counts;
- comparator/ADES geometric-mean total-time ratios;
- comparator/ADES geometric-mean query-time ratios;
- 95% Student-t intervals in log-ratio space;
- total/query loss-cell counts;
- per-experiment minimum cell ratios;
- PR62's `locality_percent=0` correction;
- PR66 cache-hit and state-cap facts;
- PR80 cache/seed/source-regime/state facts.

A successful reproduction must retain the claim boundaries `NO_GO_UNCHANGED` for broad algorithmic novelty and `NOT_ESTABLISHED` for population inference and universal/theoretical SOTA.

## 4. Expected independently recomputed headline values

| Evidence | Exact | Total GM | Query GM | Total losses | Query losses |
|---|---:|---:|---:|---:|---:|
| PR62 | 24/24 | `5.112681278031799` | `5.0593925992674595` | 0 | 0 |
| PR66 | 24/24 | `15.561191443141636` | `3.897865818296903` | 0 | 0 |
| PR80 | 24/24 | `157.67741734463613` | `30.199378953227633` | 0 | 0 |

PR62 must remain interpreted narrowly: all 24 executed workload records have `locality_percent=0`, so PR62 does **not** establish source-policy breadth. PR66 is the experiment that re-earned source-reuse breadth against VeloGraphX. PR80 adds preregistered non-road topology and fresh graph/workload seeds.

## 5. Evidence and source map

- `paper/ADES_MANUSCRIPT.md` — citation-complete submission manuscript.
- `paper/EXPERIMENT_ENVIRONMENT.md` — machine/environment provenance and cross-campaign hardware limitation.
- `paper/references.bib` — revalidated bibliography.
- `docs/PUBLICATION_CONTRACT.md` — publication claim/evidence contract.
- `docs/ALGORITHM.md` — reconciled implementation-level algorithm description.
- `benchmarks/comparators/registry.json` — qualified comparator registry.
- `evidence/pr87_publication_manifest.json` — machine-readable publication evidence index.
- `evidence/pr86_prior_art_novelty_ledger.json` — conservative novelty/prior-art classification.
- `tools/reproduce_publication_evidence.py` — independent per-cell evidence verifier.

## 6. What PR88 does and does not prove

PR88 is an **artifact-level independent recomputation**. It proves that the pinned raw benchmark artifacts regenerate the reported headline statistics and selected provenance facts without trusting their archived aggregate summaries.

PR88 is **not** an independent third-party hardware replication. A stronger replication would rebuild the frozen commits and rerun the frozen workloads on separately administered hardware.

## 7. Reviewer falsification targets

A reviewer should treat any of the following as material evidence problems:

- artifact ZIP digest does not match its pinned SHA-256;
- any central cell is not exact;
- total/query loss-cell counts differ from the frozen evidence;
- a material per-cell recomputation disagrees with the publication manifest;
- VeloGraphX exceeds the declared logical-state cap in a matched cell;
- the adapter is described as native upstream arbitrary-source support;
- the 64 MiB cap is represented as equal actual memory/RSS;
- PR62 is used to claim source-reuse breadth;
- slice-level intervals are presented as population-level inference;
- the manuscript promotes broad algorithmic novelty despite the PR86 `NO_GO` verdict.

The intended artifact posture is falsifiable: a contradiction should reopen the relevant evidence stage rather than be repaired by wording around it.
