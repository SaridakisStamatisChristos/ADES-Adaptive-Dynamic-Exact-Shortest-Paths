# Appendix A. Experimental Environment Provenance

The central evidence campaigns were executed on GitHub-hosted Linux x86-64 runners. Environment records are retained inside the canonical workflow artifacts listed in the main manuscript. This appendix exposes the machine provenance that is relevant to interpreting timing results; it does not add or reinterpret performance evidence.

| Evidence campaign | Canonical run | Canonical artifact | Runner / virtualization | Visible CPU allocation | CPU model | Recorded toolchain details |
|---|---:|---:|---|---:|---|---|
| PR62 Boost.Graph New York holdout | `38711826306` | `11961213712` | Linux x86-64 Azure VM | 4 logical CPUs | AMD EPYC 9V45 96-Core Processor | machine provenance retained in artifact |
| PR66 VeloGraphX New York source-reuse holdout | `38730045262` | `11971060916` | Linux x86-64 Azure VM | 4 logical CPUs | AMD EPYC 7763 64-Core Processor | machine provenance retained in artifact |
| PR80 VeloGraphX non-road holdout | `38856829047` | `12027088809` | GitHub-hosted Ubuntu 24.04 Linux x86-64 runner on Azure | 4 logical CPUs | AMD EPYC 7763 64-Core Processor | Python 3.12.3; GCC 13.3.0; CMake 3.31.6 |

For PR80, the archived workflow log additionally records Ubuntu 24.04.5 LTS, GitHub runner version `2.337.0`, runner image `ubuntu-24.04` version `20261012.334.1`, and Azure `westcentralus`. The environment record pins the frozen ADES candidate, the VeloGraphX upstream commit, adapter and pair-driver identities, state budget, graph/workload seeds, and oracle-validation declaration.

## A.1 Interpretation of hardware variation

The three evidence campaigns were **not all executed on the same physical CPU model**: PR62 reports an AMD EPYC 9V45, whereas PR66 and PR80 report an AMD EPYC 7763. All headline ratios are matched comparator/ADES comparisons within the same experiment cell/run and use the experiment-specific execution-order controls defined by their contracts. Therefore the central inference is a within-campaign relative comparison.

The paper does **not** compare absolute latency values across PR62, PR66, and PR80 as though those campaigns shared an identical host. In particular, the larger ratio in PR80 must not be interpreted as a calibrated cross-hardware progression from PR62 or PR66.

## A.2 Cloud-runner limitations

The runners are virtualized hosted machines rather than dedicated bare-metal benchmarking hosts. Cloud scheduling, background activity, CPU-frequency behavior, and host placement are possible sources of timing variability. The campaign mitigates these risks through matched same-cell comparisons, reversed execution order where specified, repeated cells, per-cell ratios, exactness gating, loss-cell reporting, and log-space aggregate summaries. These controls reduce but do not eliminate the limitations of virtualized benchmark hardware.

A stronger external validation would rerun the frozen candidate and comparator contracts on separately administered dedicated hardware while preserving the same graph/trace identities and accounting rules.
