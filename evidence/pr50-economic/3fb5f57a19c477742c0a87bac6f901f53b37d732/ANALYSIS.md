# PR50 Predictive Economic Residency — Benchmark Analysis

## Evidence classification

- `development` reuses the PR49 matrix and is tuning/regression evidence, not independent confirmation.
- `holdout` uses unseen graph classes, workload families, seeds, and update modes.
- Performance losses are preserved; only exactness/budget/reproducibility failures invalidate execution.

## Result summary

- development_regimes: 36
- holdout_regimes: 72
- development_v2_fastest_or_within_1pct: 22
- holdout_v2_fastest_or_within_1pct: 34
- development_material_regressions: 6
- holdout_material_regressions: 1
- development_material_wins: 33
- holdout_material_wins: 145
- holdout_zero_material_regressions: False

## Fastest-profile counts

- development:ADES: 4
- development:ADES-V2: 19
- development:B3L: 6
- development:COLD: 4
- development:FREQ-LRU-REPAIR: 3
- holdout:ADES: 17
- holdout:ADES-V2: 28
- holdout:COLD: 27

## Largest holdout regressions vs ADES-V2

- syn-clustered-50000-c100-d6-v1 / hot-pool / repeated-edge / 1/2 / 16777216 bytes / ADES: comparator/v2=0.808387911 (95% CI 0.707710862–0.923387008)
- syn-uniform-50000-d6-v1 / rotating-hot / repeated-edge / 1/2 / 67108864 bytes / COLD: comparator/v2=0.863204355 (95% CI 0.744276760–1.001135327)
- syn-clustered-50000-c100-d6-v1 / hot-pool / repeated-edge / 1/2 / 16777216 bytes / COLD: comparator/v2=0.863981736 (95% CI 0.571959360–1.305100488)
- syn-clustered-50000-c100-d6-v1 / zipf / bursty / 1/10 / 67108864 bytes / COLD: comparator/v2=0.877741344 (95% CI 0.574046825–1.342102829)
- syn-uniform-50000-d6-v1 / rotating-hot / repeated-edge / 1/2 / 16777216 bytes / ADES: comparator/v2=0.885569972 (95% CI 0.739148626–1.060996595)
- syn-small-world-50000-k8-r10-v1 / hot-pool / bursty / 1/10 / 16777216 bytes / COLD: comparator/v2=0.905660023 (95% CI 0.767060896–1.069302425)
- syn-small-world-50000-k8-r10-v1 / hot-pool / repeated-edge / 1/10 / 67108864 bytes / COLD: comparator/v2=0.906547939 (95% CI 0.812760480–1.011157883)
- syn-clustered-50000-c100-d6-v1 / zipf / bursty / 1/10 / 67108864 bytes / ADES: comparator/v2=0.908423853 (95% CI 0.619253746–1.332626410)
- syn-uniform-50000-d6-v1 / zipf / repeated-edge / 1/10 / 67108864 bytes / COLD: comparator/v2=0.908430598 (95% CI 0.725437781–1.137583641)
- syn-uniform-50000-d6-v1 / rotating-hot / repeated-edge / 1/2 / 67108864 bytes / ADES: comparator/v2=0.909975376 (95% CI 0.573324367–1.444304888)
- syn-uniform-50000-d6-v1 / hot-pool / repeated-edge / 1/10 / 16777216 bytes / ADES: comparator/v2=0.934567386 (95% CI 0.770347284–1.133795389)
- syn-small-world-50000-k8-r10-v1 / rotating-hot / bursty / 1/2 / 67108864 bytes / COLD: comparator/v2=0.940483454 (95% CI 0.764578616–1.156858311)
