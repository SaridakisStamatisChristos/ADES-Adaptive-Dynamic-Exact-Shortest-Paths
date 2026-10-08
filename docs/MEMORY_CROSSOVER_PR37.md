# PR37 — Matched-cap memory crossover

Workflow: `.github/workflows/memory-crossover.yml`. Driver: `tools/memory_crossover.py`.

The runner executes **B2L, B3L and B4 as separate processes**, using identical graph, generated query/update trace parameters, seed and resident-source capacity per matched cell. Each baseline independently generates and verifies the same trace using the existing phase runner's Dijkstra oracle. The run order rotates by cell/repetition to reduce systematic fixed-order bias.

Outputs under `results/memory-crossover/`:
- `manifest.json`: graph SHA-256, commit, platform, run parameters and evidence classification.
- `measurements.csv`: baseline, cell, repeat, trace parameters, algorithm nanoseconds (from the phase CSV), wall seconds, per-process peak RSS KiB (GNU time), exit status and existing baseline counters.
- Per-invocation `.stdout`, `.stderr` and `.time` files; artifact upload also runs after failure.

**Scope and caveats:** This is a *matched resident-source-cap* diagnostic, not a proven equal-byte-memory experiment. RSS includes graph loading, trace generation, oracle and temporary allocations, and may peak outside the timed algorithm section. Algorithm nanoseconds come from the original phase runner's timed `run()` and do not include setup/oracle. Wall time and peak RSS measure the full process. No runtime ratio or scientific crossover conclusion is automatically inferred. A failed cell aborts the run, and the partial data must not be interpreted as complete comparisons.

Default dispatch: 10 queries, 2 repeats, caps 2/8, seeds 7/17, uniform/single-hot families, update intervals 0/10; 96 invocations. Start with defaults before increasing workloads. Runner resource availability and statistical power must be evaluated before any publication claim.

Original B2/B3, B4 algorithm, benchmark schema, and legacy `ALL` selection are unchanged. No releases or tags.
