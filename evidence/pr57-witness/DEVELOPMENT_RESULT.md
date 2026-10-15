# PR57 — Alternate-Witness Root Rescue: Development Result

## Evidence classification

**Targeted mechanism ablation.** This evidence establishes the causal effect of alternate-witness root rescue on deliberately constructed selected-parent increase cases. It is **not** a broad workload, publication-holdout, or SOTA result.

- Frozen causal baseline: PR55 `940c5a16237ddf017925471b185b74f001503b8c`
- PR57 measured head: `9e3139ee8383f59d640a958b15d884e6832ae28b`
- Pull request: #57
- GitHub Actions run: `38672114912`
- Job: `116217415473`
- Artifact ID: `11944268151`
- Artifact SHA-256: `4802268ae4414bc8c5ac68f13e08739763065db53f71ab2c34345ab3be6bcd31`
- Independent seeds: `7, 17, 29`
- Repetitions per seed: `2`, with PR55/PR57 execution order reversed on repetition 2

## Correctness gate

The candidate was built in Release mode and the complete CTest suite passed:

```text
100% tests passed, 0 tests failed out of 9
```

The new PR57 test suite covers:

- strict-distance alternate-witness rescue before subtree discovery;
- equal-distance/zero-weight rescue only after read-only subtree membership discovery;
- a zero-weight descendant/cycle candidate that must not be accepted as an independent witness;
- randomized selected-parent weight increases checked against fresh Dijkstra.

The matched benchmark also validates candidate distances against fresh Dijkstra on the first and last round of every isolated execution.

## Mechanism result

`PR55 / PR57 > 1` means PR57 is faster.

| case | PR55 / PR57 geomean | 95% CI | PR55 mean discovered | PR57 mean discovered | verdict |
|---|---:|---:|---:|---:|---|
| chain-4096 | 2132.197177× | 1264.294406–3595.890944 | 4094.0 | 0.0 | IMPROVEMENT |
| binary-65536 | 14072.478274× | 12351.285338–16033.525204 | 65534.0 | 0.0 | IMPROVEMENT |

Machine result: **2 improvements, 0 regressions, 0 inconclusive**.

## Interpretation

These large ratios are expected for the constructed mechanism cases and must not be generalized to ordinary workload speedup. PR55 discovers and repairs an affected selected-SPT subtree even though an unchanged equally short predecessor already certifies the root distance. PR57 finds that witness in a bounded incoming-edge probe, reparents the root, and therefore preserves every descendant witness without discovering the subtree.

The important causal result is not the headline ratio; it is that the affected work collapses from `O(|affected subtree| + repair work)` to a bounded witness probe when the exact rescue condition holds.

## Safety / bounded-overhead properties

- The fast probe examines at most 64 incoming arcs.
- Failure to find a valid witness falls back to the unchanged PR55 exact-repair path.
- Strict-distance rescue requires `dist[u] < dist[v]`, which excludes descendants of `v` under nonnegative weights.
- Equal-distance rescue requires the predecessor to lie outside the read-only marked affected subtree.
- Witness probe counters are excluded from the historical `RepairWork::total()` units, so this PR does not silently recalibrate the PR55 learned repair controller.
- No additional persistent shortest-path DAG or resident-memory structure is introduced.

## Disposition

**MECHANISM SURVIVES.** Keep PR57 as a candidate and proceed to a broad end-to-end regression/impact matrix on ordinary generated workloads. Accepting PR57 into the final ADES candidate requires that the bounded probe does not create material regressions where rescue opportunities are rare.

## Claim boundary

This result does not establish practical or theoretical SOTA. A stronger claim still requires broad held-out workloads, explicit persistent-memory parity, and DIRECT-qualified external comparators under the frozen comparator contract.
