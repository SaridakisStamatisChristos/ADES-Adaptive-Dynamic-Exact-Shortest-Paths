# PR54 matched core benchmark — development result

## Evidence classification

**Development evidence only.** This run is not a fresh publication holdout and does not establish universal superiority.

- Candidate head measured: `8f02e062f2cca55409e81017cfee946dbdbc09d9`
- Baseline: `main` at `333764c230ee05b1da1f5630133538c9e02b7b26`
- GitHub Actions run: `38222773463`
- Artifact ID: `11735330956`
- Artifact SHA-256: `a4b7a8b8966b71306b07fbd8c7ae16915b7d6fe7662cc777adf9b07eb5712a8a`
- Independent seeds: `7, 17, 29`
- Repetitions per seed: `2`, with candidate/baseline execution order reversed on repetition 2
- Exact trace identity, independent baseline-generated oracle agreement, and byte-budget enforcement were hard requirements.

The artifact contains raw `measurements.csv`, `summary.csv`, `decision.json`, canonical traces/metadata, and oracle bundles. The deterministic PR54 driver can reproduce the development matrix.

## Results

`baseline_over_candidate_geomean > 1` means PR54 is faster.

| case | profile | geomean speedup | 95% CI | verdict |
|---|---|---:|---:|---|
| grid-hotpool-u10 | COLD | 1.103819× | 1.073821–1.134656 | IMPROVEMENT |
| grid-hotpool-u10 | FREQ-LRU-REPAIR | 1.109404× | 1.058060–1.163240 | IMPROVEMENT |
| grid-hotpool-u10 | ADES-V3 | 1.109209× | 1.074802–1.144717 | IMPROVEMENT |
| grid-churn-u5 | COLD | 1.094045× | 1.091329–1.096768 | IMPROVEMENT |
| grid-churn-u5 | FREQ-LRU-REPAIR | 1.143267× | 1.100521–1.187673 | IMPROVEMENT |
| grid-churn-u5 | ADES-V3 | 1.053558× | 0.864940–1.283309 | INCONCLUSIVE |

Machine summary: **5 improvements, 0 regressions, 1 inconclusive**.

## Memory-accounting result

On the 50,176-vertex grid with four resident SSSP states, representative peak logical state changed from approximately:

- baseline: `8,833,408` bytes;
- PR54: `4,819,328` bytes.

The difference is exactly `4,014,080` bytes = `20 bytes × 50,176 vertices × 4 residents`, matching the representation change from the previous 44-byte per-vertex resident payload (distance + four 64-bit SPT IDs + resident repair mark) to the PR54 24-byte semantic payload (distance + four 32-bit SPT IDs, with membership scratch shared outside resident state).

This reduction is exposed through persistent-state accounting version 2 and therefore legitimately changes how much semantic state can fit under a fixed byte budget.

## Preserved negative detail

The inconclusive `grid-churn-u5 / ADES-V3` cell is not hidden. In seed 29 the PR54 candidate made one additional repair-abort/rebuild decision:

- baseline: 4 rebuilds, 0 repair aborts;
- candidate: 5 rebuilds, 1 repair abort.

That seed was slower for PR54 even though the aggregate point estimate across three seeds remained favorable. The likely mechanism is controller sensitivity to the changed post-PR54 build/repair cost landscape, not an exactness or byte-budget failure. This is evidence for recalibrating the next-generation controller **after** PR54 rather than tuning the old controller inside this core-engine PR.

## Disposition

The core-engine hypothesis survives this development matrix:

- common cold search improves materially;
- repair-oriented execution improves materially;
- the merged ADES-V3 controller benefits strongly in hot-pool traffic;
- no statistically material regression is observed in the tested cells;
- one controller-sensitive churn cell remains uncertain and is explicitly carried forward.

A fresh post-freeze multi-topology holdout is still required before any stronger performance claim.
