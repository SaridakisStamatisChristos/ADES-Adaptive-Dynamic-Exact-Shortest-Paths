# PR51 ADES-v3 Frozen Evidence

This directory preserves the completed PR51 full scientific evaluation.

- Execution commit: `8363cd090686c00eb7ffad137f2579c3e44961fe`
- Frozen controller source: `95163a3f260548439756abd20075813933715705`
- GitHub Actions run: `37897020318`
- Artifact: `ades-pr51-fast-economic-full` (`11600484717`)
- Artifact digest: `sha256:cb733f28bfab07e45f851cffb7b9e0bae3c5973d16864634aa7cfefd41e50e0e`
- Corrected full trace length: 120 queries. The prior 60-query attempt was rejected before performance evidence because interval 1/100 produced zero updates and duplicate trace identities.

## Frozen result

ADES-V3 is the most frequent outright winner on the fresh second holdout, but the predeclared zero-material-regression target is **not met**.

- holdout2 regimes: 48
- ADES-V3 fastest: 20
- ADES-V2 fastest: 15
- COLD fastest: 10
- ADES-v1 fastest: 1
- FREQ-LRU-REPAIR fastest: 2
- ADES-V3 fastest or within 1%: 26 / 48
- material V3 pairwise wins: 105
- material V3 regressions: 6

The six material regressions are primarily ADES-V2 wins in NY/hot-pool and scale-free/hot-pool cells. Mechanistically, V3's early-promotion policy can over-materialize: on NY 24 MiB cells V3 performs roughly 14–17 promotions with 12–15 evictions where V2 typically performs only 2–3 promotions; on several scale-free cells V2 remains cold while V3 builds resident states. This is preserved negative evidence.

The second holdout is now consumed and must not be relabeled as fresh confirmation after any further tuning.

`compact-evidence.tgz` preserves the browsable tables plus all canonical regression/holdout2 traces and exact oracle bundles used by the run.