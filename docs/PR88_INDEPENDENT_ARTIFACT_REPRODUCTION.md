# PR88 — Independent artifact reproduction

## Purpose

PR87 assembled the submission-facing evidence package. PR88 tests whether its central numerical claims can be regenerated from the canonical GitHub Actions artifact ZIPs without trusting the archived aggregate verdict files.

This is an **evidence reproduction** step. It does not change ADES, its comparators, workload generators, frozen benchmark inputs, or any scientific claim boundary.

## Frozen inputs

The verifier re-downloads the exact three central artifacts identified by `evidence/pr87_publication_manifest.json`:

| Evidence | Run | Artifact | Canonical ZIP SHA-256 |
|---|---:|---:|---|
| PR62 Boost.Graph NY holdout | `38711826306` | `11961213712` | `0bd64d43ba48fda4c9e0752ea6acafc1c9fa8ddb7eec2f2fdabc88d1574029ff` |
| PR66 VeloGraphX NY source-reuse holdout | `38730045262` | `11971060916` | `c4a3d64af948afd03daa1e983ee2d75eadf838ca94bdc59103c25ff5575dd299` |
| PR80 VeloGraphX non-road holdout | `38856829047` | `12027088809` | `617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100` |

The frozen ADES candidate remains:

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

## Independence rule

`tools/reproduce_publication_evidence.py` deliberately does **not** read `holdout_summary.json` or any archived top-level aggregate verdict. It reads the per-cell CSV rows directly:

- PR62: `cells.csv`;
- PR66: `cells.csv`;
- PR80: `results/cells.csv`.

From those rows it independently recomputes:

1. exact-cell count;
2. geometric mean comparator/ADES ratio for total algorithm time;
3. geometric mean comparator/ADES ratio for query time;
4. 95% Student-t intervals in log-ratio space (`n=24`, `df=23`);
5. total/query loss-cell counts;
6. minimum total/query cell ratios.

It additionally checks evidence-specific facts:

- **PR62:** all 24 workload JSON records have `locality_percent=0`; therefore the mandatory correction that PR62 does not establish source-policy breadth remains active.
- **PR66:** hot-pool and rotating-hot VeloGraphX cache hits are re-summed from cell rows, and VeloGraphX accounted persistent state remains below the 64 MiB cap.
- **PR80:** cache hits/misses/evictions, graph seeds, workload seeds, source regimes, and peak accounted state are reconstructed from cell rows; the slice-level/non-population uncertainty boundary is preserved.

## Independent preflight executed before PR creation

Fresh downloads of the three canonical artifacts produced ZIP hashes identical to PR87. Direct per-cell recomputation produced:

| Evidence | Exact | Total GM | Total 95% interval | Query GM | Query 95% interval | Total losses | Query losses |
|---|---:|---:|---:|---:|---:|---:|---:|
| PR62 | 24/24 | `5.112681278031799` | `[4.851231267414431, 5.388221754405966]` | `5.0593925992674595` | `[4.800065020634559, 5.332730569999321]` | 0 | 0 |
| PR66 | 24/24 | `15.561191443141636` | `[14.569886971565762, 16.61994218642052]` | `3.897865818296903` | `[2.9820188077121643, 5.09499064799792]` | 0 | 0 |
| PR80 | 24/24 | `157.67741734463613` | `[95.32915393098806, 260.8034049947942]` | `30.199378953227633` | `[16.21826104546237, 56.23306263255717]` | 0 | 0 |

The very small interval-endpoint differences from archived summaries are floating-point/library rounding only; the PR87 manifest intentionally rounds PR62/PR66 headline values to six decimals. PR88 uses a tight tolerance that accepts those recorded roundings but would reject any material numerical drift.

Additional independently reconstructed facts:

- PR66 VeloGraphX cache hits: hot-pool-2 `588`, hot-pool-4 `576`, rotating-hot-4 `576`, uniform `0`;
- PR66 maximum VeloGraphX accounted persistent state: `55,185,520` bytes;
- PR80 VeloGraphX cache totals: `996` hits, `444` misses, `204` evictions;
- PR80 maximum accounted persistent state: VeloGraphX `65,507,832` bytes, ADES `1,802,576` bytes;
- PR80 graph seeds: `8209`, `9103`, `10037`; workload seeds: `12007`, `13001`.

## PR88 acceptance gate

The canonical PR88 workflow must:

1. fetch each artifact anew from GitHub by pinned artifact ID;
2. verify the ZIP SHA-256 before parsing;
3. execute the independent per-cell reproducer against the PR87 manifest;
4. require `status = PASS` and the unchanged scientific boundaries;
5. upload the generated reproduction report and ZIP-hash ledger as a retained Actions artifact;
6. prove the PR touches only the reproducer, this document, and the PR88 workflow.

Any digest mismatch, missing cell, exactness failure, loss-cell drift, material ratio/interval mismatch, memory-cap violation, cache/seed mismatch, or claim-boundary promotion is a **hard failure**.

## Scientific disposition

If the gate passes, PR88 establishes only this stronger provenance statement:

> The central PR87 performance evidence is independently reproducible from the pinned raw GitHub Actions artifacts at the per-cell level.

It does **not** establish universal/theoretical SOTA, population-level inference, or broad algorithmic novelty. PR86's broad-novelty `NO_GO` remains unchanged.
