# Crossover descriptive analysis

- Measurement rows: **144**
- Valid rows: **144**
- Complete matched B2L/B3L/B4 pairs: **48**
- Every valid invocation contains both increase and decrease updates: **yes**

Speedup is comparator algorithm time divided by B4 algorithm time; values above 1 favor B4. RSS ratio is comparator peak RSS divided by B4 peak RSS; values above 1 mean B4 used less measured peak RSS.

| mode | family | update every | cap | pairs | updates | B2L ms | B3L ms | B4 ms | B4/B2L speedup | B4/B3L speedup | B4 RSS MiB |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| alternating | hot-pool | 5 | 2 | 4 | 19.0 | 5584.189 | 3509.396 | 1571.806 | 3.553 | 2.278 | 119.283 |
| alternating | hot-pool | 5 | 8 | 4 | 19.0 | 7967.026 | 762.556 | 772.053 | 10.568 | 1.003 | 141.512 |
| alternating | hot-pool | 10 | 2 | 4 | 9.0 | 4410.295 | 3374.785 | 1546.322 | 2.852 | 2.190 | 119.299 |
| alternating | hot-pool | 10 | 8 | 4 | 9.0 | 4257.358 | 970.425 | 842.170 | 5.114 | 1.153 | 140.887 |
| alternating | single-hot | 5 | 2 | 4 | 19.0 | 2394.618 | 395.510 | 222.908 | 10.671 | 1.775 | 108.172 |
| alternating | single-hot | 5 | 8 | 4 | 19.0 | 4840.487 | 419.379 | 224.956 | 21.629 | 1.866 | 107.400 |
| alternating | single-hot | 10 | 2 | 4 | 9.0 | 1382.765 | 432.239 | 223.048 | 6.319 | 1.927 | 107.217 |
| alternating | single-hot | 10 | 8 | 4 | 9.0 | 2860.024 | 476.419 | 247.193 | 11.665 | 1.936 | 107.662 |
| alternating | uniform | 5 | 2 | 4 | 19.0 | 7526.774 | 5497.438 | 1589.541 | 4.674 | 3.448 | 103.525 |
| alternating | uniform | 5 | 8 | 4 | 19.0 | 13657.806 | 5600.669 | 1647.432 | 8.303 | 3.394 | 103.547 |
| alternating | uniform | 10 | 2 | 4 | 9.0 | 6406.663 | 5408.102 | 1597.781 | 4.015 | 3.400 | 103.516 |
| alternating | uniform | 10 | 8 | 4 | 9.0 | 9325.619 | 5488.836 | 1584.735 | 5.922 | 3.535 | 104.041 |

These are descriptive medians from the recorded matched runs, not inferential confidence intervals or a state-of-the-art claim. The experiment matches resident-source capacity, not exact byte-level memory budgets.
