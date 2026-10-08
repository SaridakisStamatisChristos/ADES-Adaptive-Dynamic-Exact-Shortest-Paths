# PR37 run 37777585809 — descriptive analysis

## Result

All 96 B2L/B3L/B4 invocations completed successfully and passed the phase runner's exactness check.

Median algorithm time and peak process RSS by source family:

| Source family | Baseline | Median algorithm time (ms) | Median peak RSS (MiB) |
|---|---:|---:|---:|
| uniform | B2L | 547.586 | 158.555 |
| uniform | B3L | 550.125 | 158.619 |
| uniform | B4 | 173.888 | 103.023 |
| single-hot | B2L | 111.567 | 114.199 |
| single-hot | B3L | 113.042 | 114.020 |
| single-hot | B4 | 136.028 | 106.213 |

For matched runs, the median baseline/B4 algorithm-time ratio was:

- uniform: B2L/B4 = **3.206x**, B3L/B4 = **3.224x** (values above 1 favor B4);
- single-hot: B2L/B4 = **0.762x**, B3L/B4 = **0.766x** (values below 1 favor B2L/B3L).

These results show a strong workload-dependent crossover in this small diagnostic: B4 is substantially faster on uniform-source traces and slower on single-hot traces, while using less median peak RSS in both families.

## Important limitation

No dynamic edge update was executed in this run. The workload had 10 queries and update intervals of 0 or 10. The phase trace generator inserts an update only when `q != 0 && q % update_every == 0`; with query indices 0..9, `update_every=10` never fires. Consequently, this run must not be cited as evidence for B4's dynamic-repair performance.

Other limitations include two seeds, two repetitions, short workloads, GitHub-hosted runner variability, and matched source capacity rather than equal byte-level memory budgets.

## Interpretation

This run is useful as a reproducibility and infrastructure milestone and as preliminary evidence for adaptive admission/cache behavior. A follow-up experiment must use longer traces with verified nonzero update counts before drawing conclusions about dynamic shortest-path performance.
