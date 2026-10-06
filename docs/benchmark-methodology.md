# Benchmark methodology

Every result must record commit SHA, compiler/version/flags, OS/kernel, CPU, RAM, thread count, seed, graph/trace metadata, controller config, warmup, repetitions, summary statistics, peak RSS, and exactness status.

Timed regions exclude trace construction, oracle validation, logging, graph/state copies, and setup allocation unless the experiment explicitly measures them. A failed exactness check invalidates its performance result.
