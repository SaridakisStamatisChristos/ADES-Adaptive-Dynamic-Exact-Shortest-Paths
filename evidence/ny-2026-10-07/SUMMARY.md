# ADES benchmark summary

Generated from raw evidence CSVs. Times are per isolated process measurement; median/p95 are descriptive, not confidence intervals.

## B0-B4 matrix

| metric | workload | baseline | n | median ms | p95 ms | median RSS MiB |
|---|---|---:|---:|---:|---:|---:|
| distance | clustered | B0 | 5 | 8330.251 | 8954.676 | 102.11 |
| distance | clustered | B1 | 5 | 17.136 | 17.481 | 95.02 |
| distance | clustered | B2 | 5 | 190121.993 | 198943.486 | 1411.89 |
| distance | clustered | B3 | 5 | 7249.164 | 7503.293 | 1412.00 |
| distance | clustered | B4 | 5 | 18.812 | 19.158 | 96.95 |
| distance | cross | B0 | 5 | 7740.873 | 8974.246 | 103.12 |
| distance | cross | B1 | 5 | 4620.234 | 5707.125 | 94.98 |
| distance | cross | B2 | 5 | 142211.135 | 148989.826 | 1067.09 |
| distance | cross | B3 | 5 | 5419.709 | 5550.329 | 1068.16 |
| distance | cross | B4 | 5 | 4554.427 | 4739.044 | 151.47 |
| distance | local | B0 | 5 | 9742.655 | 9851.010 | 102.00 |
| distance | local | B1 | 5 | 17.602 | 17.672 | 94.94 |
| distance | local | B2 | 5 | 171095.021 | 174691.754 | 1067.91 |
| distance | local | B3 | 5 | 6414.457 | 6592.833 | 1068.09 |
| distance | local | B4 | 5 | 18.482 | 18.587 | 96.98 |
| distance | mixed | B0 | 5 | 7574.389 | 9005.866 | 103.22 |
| distance | mixed | B1 | 5 | 2561.483 | 3060.172 | 96.25 |
| distance | mixed | B2 | 5 | 216541.309 | 239344.252 | 1778.14 |
| distance | mixed | B3 | 5 | 8271.077 | 8677.482 | 1777.21 |
| distance | mixed | B4 | 5 | 2537.832 | 3068.027 | 98.17 |
| distance | moving | B0 | 5 | 6692.144 | 7116.221 | 103.06 |
| distance | moving | B1 | 5 | 18.638 | 18.756 | 94.93 |
| distance | moving | B2 | 5 | 170773.984 | 172641.651 | 1776.99 |
| distance | moving | B3 | 5 | 7596.423 | 7652.824 | 1777.08 |
| distance | moving | B4 | 5 | 19.345 | 19.487 | 98.01 |
| time | clustered | B0 | 5 | 8677.787 | 9055.406 | 102.05 |
| time | clustered | B1 | 5 | 17.437 | 18.061 | 95.05 |
| time | clustered | B2 | 5 | 192302.349 | 193778.286 | 1411.93 |
| time | clustered | B3 | 5 | 7432.814 | 7491.644 | 1412.06 |
| time | clustered | B4 | 5 | 18.067 | 18.758 | 98.01 |
| time | cross | B0 | 5 | 9136.138 | 9633.262 | 103.12 |
| time | cross | B1 | 5 | 5346.124 | 5607.301 | 95.04 |
| time | cross | B2 | 5 | 163739.775 | 168998.276 | 1068.01 |
| time | cross | B3 | 5 | 5543.364 | 5702.950 | 1067.20 |
| time | cross | B4 | 5 | 5601.522 | 5702.611 | 152.45 |
| time | local | B0 | 5 | 8615.465 | 8868.021 | 103.09 |
| time | local | B1 | 5 | 20.955 | 21.084 | 94.96 |
| time | local | B2 | 5 | 227422.966 | 229921.255 | 1765.98 |
| time | local | B3 | 5 | 9242.132 | 9437.706 | 1766.92 |
| time | local | B4 | 5 | 21.872 | 22.343 | 96.93 |
| time | mixed | B0 | 5 | 8479.122 | 8875.527 | 103.05 |
| time | mixed | B1 | 5 | 1932.550 | 2239.614 | 95.08 |
| time | mixed | B2 | 5 | 216544.557 | 224955.355 | 1777.94 |
| time | mixed | B3 | 5 | 9357.893 | 9561.577 | 1778.06 |
| time | mixed | B4 | 5 | 2126.980 | 2150.664 | 96.91 |
| time | moving | B0 | 5 | 9315.050 | 9411.546 | 103.07 |
| time | moving | B1 | 5 | 20.033 | 21.030 | 95.98 |
| time | moving | B2 | 5 | 225348.012 | 232734.123 | 1777.00 |
| time | moving | B3 | 5 | 9776.052 | 9890.070 | 1778.09 |
| time | moving | B4 | 5 | 21.354 | 22.294 | 98.07 |

## Controller ablation

| metric | regime | policy | n | median ms | p95 ms | median RSS MiB | rebuilds | repairs | aborts |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| distance | catastrophic | fixed | 5 | 945.395 | 993.110 | 172.58 | 5 | 100 | 0 |
| distance | catastrophic | vertex | 5 | 1207.131 | 1221.588 | 173.47 | 105 | 0 | 100 |
| distance | catastrophic | work | 5 | 1222.685 | 1258.548 | 171.54 | 105 | 0 | 100 |
| distance | small | fixed | 5 | 0.238 | 0.243 | 170.51 | 5 | 100 | 0 |
| distance | small | vertex | 5 | 0.238 | 0.296 | 170.46 | 5 | 100 | 0 |
| distance | small | work | 5 | 0.222 | 0.233 | 169.61 | 5 | 100 | 0 |
| time | catastrophic | fixed | 5 | 735.442 | 782.014 | 172.55 | 5 | 100 | 0 |
| time | catastrophic | vertex | 5 | 965.897 | 1078.149 | 172.55 | 105 | 0 | 100 |
| time | catastrophic | work | 5 | 1018.436 | 1085.382 | 170.52 | 105 | 0 | 100 |
| time | small | fixed | 5 | 0.112 | 0.115 | 170.52 | 5 | 100 | 0 |
| time | small | vertex | 5 | 0.111 | 0.118 | 170.57 | 5 | 100 | 0 |
| time | small | work | 5 | 0.112 | 0.113 | 170.57 | 5 | 100 | 0 |
