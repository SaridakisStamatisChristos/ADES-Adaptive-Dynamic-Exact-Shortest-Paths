# PR56 — Predictive Repair-vs-Rebuild Preflight: Development Result

## Evidence classification

**Development controller experiment.** PR55 is the frozen causal baseline. This result does not establish a broad performance improvement or SOTA.

- Frozen PR55 baseline: `940c5a16237ddf017925471b185b74f001503b8c`
- PR56 measured branch head: `4ef89e8380c550e361fa43452bbb6c7bf6dc9ce7`
- Pull request: #56
- GitHub Actions run: `38672161424`
- Job: `116217564758`
- Artifact ID: `11943658490`
- Artifact SHA-256: `6530ef3c4250556ea4c52068b702c4b3fb2f6dc40c8ae621a949181a56f8aad5`
- Independent seeds: `7, 17, 29`
- Repetitions per seed: `2`, reversed execution order
- Rounds per execution: `40`

The previously failed corpus acquisition was repaired with retry-all-errors, timeouts, atomic `.part` downloads, and checksum verification. On the measured run all three canonical NY files verified successfully before benchmarking.

## Correctness gate

Release build completed and the complete PR56 test suite passed:

```text
100% tests passed, 0 tests failed out of 9
```

## Performance result

`PR55 / PR56 > 1` means PR56 is faster.

| regime | geomean PR55 / PR56 | 95% CI | verdict |
|---|---:|---:|---|
| small | 1.103110× | 0.991727–1.227003 | INCONCLUSIVE |
| catastrophic | 1.457043× | 0.408244–5.200258 | INCONCLUSIVE |

Machine result: **0 improvements, 0 regressions, 2 inconclusive**.

Seed-level speedups:

- small: `1.0607×`, `1.1548×`, `1.0959×`;
- catastrophic: `1.1457×`, `2.6236×`, `1.0291×`.

## Mechanism evidence

The small regime did not materially change decisions: both PR55 and PR56 performed one rebuild and forty repairs per execution with no repair aborts.

The catastrophic regime changed the controller substantially:

- PR55 consistently performed `41` rebuilds, `0` completed repairs, and `40` repair aborts per execution.
- PR56 seed 7 completed `18–22` repairs and reduced rebuilds to `19–23`.
- PR56 seed 17 completed `39` repairs and reduced rebuilds to `2`, with only `1` abort.
- PR56 seed 29 completed `11–12` repairs and reduced rebuilds to `29–30`.

This confirms that predictive preflight can eliminate a large fraction of the historical abort/rebuild behavior, but the timing benefit is heterogeneous enough that the present three-seed experiment cannot establish a statistically supported speedup.

## Disposition

**MECHANISM SURVIVES; PERFORMANCE CLAIM DOES NOT.** Keep PR56 experimental rather than merging it as a proven optimization. The next discriminating experiment should measure repair-vs-rebuild oracle regret directly: clone the same pre-update resident state, execute both actions, and quantify how often/how much the controller loses relative to `min(T_repair, T_rebuild)`. A richer cost model is justified only if that regret is material.

## Claim boundary

Do not describe PR56 as a performance improvement from this evidence. Both measured regimes are statistically inconclusive. The result is useful because it demonstrates a real change in controller decisions and sharply narrows the next experiment.
