# PR51 ADES-v3 — Regression + Second Holdout Analysis

## Evidence classification

- `regression` reuses previously observed failure cells and is tuning/regression evidence only.
- `holdout2` was frozen before observing ADES-v3 performance and uses new seeds, budgets, update modes/magnitudes and graph/workload pairings.
- Performance losses are preserved; exactness, identity and byte-budget failures are hard failures.

## Result summary

- regression_regimes: 9
- holdout2_regimes: 48
- regression_v3_fastest_or_within_1pct: 1
- holdout2_v3_fastest_or_within_1pct: 26
- regression_material_regressions: 6
- holdout2_material_regressions: 6
- regression_material_wins: 25
- holdout2_material_wins: 105
- holdout2_zero_material_regressions: False

## Fastest-profile counts

- holdout2:ADES: 1
- holdout2:ADES-V2: 15
- holdout2:ADES-V3: 20
- holdout2:COLD: 10
- holdout2:FREQ-LRU-REPAIR: 2
- regression:ADES-V2: 2
- regression:ADES-V3: 1
- regression:B3L: 6

## Largest second-holdout regressions vs ADES-V3

- ny-road-distance / hot-pool@70 / balanced-random:small / 1/100 / 25165824 bytes / ADES-V2: comparator/v3=0.691678012 (95% CI 0.505559205–0.946315421)
- ny-road-distance / hot-pool@70 / increase-only:large / 1/100 / 25165824 bytes / ADES-V2: comparator/v3=0.703817729 (95% CI 0.626828988–0.790262425)
- ny-road-distance / hot-pool@70 / balanced-random:small / 1/10 / 25165824 bytes / ADES-V2: comparator/v3=0.743871181 (95% CI 0.687188012–0.805229900)
- ny-road-distance / hot-pool@70 / increase-only:large / 1/10 / 25165824 bytes / ADES-V2: comparator/v3=0.754212266 (95% CI 0.518669372–1.096722060)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / balanced-random:small / 1/10 / 50331648 bytes / ADES-V2: comparator/v3=0.783362505 (95% CI 0.609601767–1.006651961)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / balanced-random:small / 1/10 / 25165824 bytes / ADES-V2: comparator/v3=0.788737979 (95% CI 0.637423987–0.975971429)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / increase-only:large / 1/10 / 25165824 bytes / ADES-V2: comparator/v3=0.791575333 (95% CI 0.618394077–1.013255999)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / increase-only:large / 1/10 / 50331648 bytes / ADES-V2: comparator/v3=0.809340291 (95% CI 0.647109134–1.012243025)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 25165824 bytes / COLD: comparator/v3=0.813780003 (95% CI 0.342683784–1.932504321)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 50331648 bytes / COLD: comparator/v3=0.817053947 (95% CI 0.347323591–1.922061068)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / balanced-random:small / 1/100 / 50331648 bytes / ADES-V2: comparator/v3=0.818300005 (95% CI 0.683591027–0.979554838)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 50331648 bytes / ADES-V2: comparator/v3=0.820467457 (95% CI 0.341906833–1.968860470)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 25165824 bytes / ADES-V2: comparator/v3=0.823247722 (95% CI 0.337601297–2.007506539)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 25165824 bytes / ADES: comparator/v3=0.823767002 (95% CI 0.336893736–2.014261475)
- syn-small-world-50000-k8-r10-v1 / rotating-hot@80 / balanced-random:small / 1/10 / 50331648 bytes / ADES: comparator/v3=0.828433038 (95% CI 0.358270441–1.915595650)
- syn-scale-free-50000-m4-v1 / hot-pool@70 / balanced-random:small / 1/100 / 25165824 bytes / ADES-V2: comparator/v3=0.829011080 (95% CI 0.671187017–1.023946162)
