# PR57 — Alternate-Witness Root Rescue: Development Result

## Evidence classification

**Causal mechanism + broad development-regression evidence.** PR55 is the frozen baseline. This file intentionally preserves both the rejected eager v1 design and the repaired v2 design; negative evidence is not overwritten.

This is **not** a publication holdout and does not establish a SOTA claim.

Frozen causal baseline:

```text
PR55 940c5a16237ddf017925471b185b74f001503b8c
```

Final measured PR57 v2 code head:

```text
fb057d83ad1c4601a7e7f45c7f44fbbea6030238
```

Pull request: `#57`.

## Mechanism

A selected-parent edge can increase even though another unchanged shortest-path witness still enters the affected root. In that case the root may be reparented without changing its distance, and every selected descendant witness remains valid.

PR57 v2 uses a deliberately conservative rescue path:

1. ordinary PR55 read-only selected-subtree discovery begins unchanged;
2. **small repairs pay no witness-probe cost at all**;
3. only after `256` affected vertices have been discovered, one bounded scan of at most `64` incoming arcs of the affected root is performed;
4. a tight predecessor with `dist[u] < dist[root]` is immediately safe because under nonnegative weights it cannot be a descendant of the root;
5. tight equal-distance candidates are retained but not accepted until complete subtree membership is known, protecting zero-weight/equal-distance cases;
6. after full discovery an equal-distance candidate is accepted only if its tail lies outside the complete affected subtree;
7. failure to find a safe witness falls back to the unchanged exact PR55 repair path.

Witness-probe counters remain outside the historical `RepairWork::total()` controller units so PR57 does not silently recalibrate PR55's learned repair policy. No persistent shortest-path DAG or additional resident semantic state is introduced.

## Correctness gate

PR57 v2 Release build and complete CTest suite:

```text
100% tests passed, 0 tests failed out of 9
```

Focused tests cover:

- strict-distance rescue after the large-repair trigger;
- explicit proof that sub-256-vertex repairs perform **zero** witness scans;
- equal-distance/zero-weight rescue only after complete read-only membership discovery;
- rejection of an unsafe equal-distance predecessor inside the affected subtree/cycle;
- randomized selected-parent increases checked against fresh Dijkstra.

The matched mechanism runner additionally validates candidate distances against fresh Dijkstra.

---

## v1 — eager probe: mechanism win, broad regression

The initial implementation probed up to 64 incoming arcs **before every selected-parent repair**.

### Targeted mechanism evidence

Run `38672114912`, job `116217415473`, artifact `11944268151`, artifact SHA-256 `4802268ae4414bc8c5ac68f13e08739763065db53f71ab2c34345ab3be6bcd31`.

Three independent seeds (`7,17,29`), two reversed-order repetitions per seed:

| case | PR55 / eager-v1 geomean | 95% CI | PR55 mean discovered | v1 mean discovered | verdict |
|---|---:|---:|---:|---:|---|
| chain-4096 | 2132.197177× | 1264.294406–3595.890944 | 4094.0 | 0.0 | IMPROVEMENT |
| binary-65536 | 14072.478274× | 12351.285338–16033.525204 | 65534.0 | 0.0 | IMPROVEMENT |

This established that the exact alternate-witness mechanism can eliminate dominated large-subtree repair work.

### Broad falsification

A subsequent 12-cell matrix covered grid / scale-free / clustered graphs, hot-pool and churn workloads, `FREQ-LRU-REPAIR` and `ADES-V3`, seeds `7,17,29`, and two reversed-order repetitions.

Run `38672389258`, job `116218274084`, artifact `11944885535`, artifact SHA-256 `b0703cbfbf8fd25064fa811406aec40e5c727d8b800a9ff1cc5f4c7be63c3296`.

Machine result:

```text
0 improvements
1 regression
11 inconclusive
```

The decisive counterexample was:

```text
clustered-hotpool-u10 / FREQ-LRU-REPAIR
PR55 / eager-v1 = 0.910073×
95% CI = 0.872704–0.949041
REGRESSION
```

**v1 disposition: REJECTED as a global default.** The mechanism was valid, but eagerly paying the probe on cheap repairs was not.

---

## v2 — large-repair-gated single probe

v2 moves the probe behind `256` already-discovered affected vertices and performs only one incoming scan. Equal-distance candidates discovered by that scan are retained until complete membership is known rather than rescanning the root later.

### Targeted mechanism evidence

Run `38672769732`, job `116219472562`, artifact `11944746185`, artifact SHA-256 `05bc16bab878d4773af50c3c8c65d08eb363fcff4516f5ab0df28baf113f97f1`.

| case | PR55 / PR57-v2 geomean | 95% CI | PR55 mean discovered | v2 mean discovered | verdict |
|---|---:|---:|---:|---:|---|
| chain-4096 | 100.614570× | 90.911248–111.353567 | 4094.0 | 256.0 | IMPROVEMENT |
| binary-65536 | 932.220446× | 837.122685–1038.121385 | 65534.0 | 256.0 | IMPROVEMENT |

Machine result:

```text
2 improvements
0 regressions
0 inconclusive
```

The huge v1 ratios intentionally shrink because v2 first pays the 256-vertex discovery threshold. The important causal result survives: once an independent alternate witness exists, very large affected-subtree work collapses to a bounded discovery prefix plus one bounded incoming probe.

### Broad regression matrix

Run `38672769724`, job `116219472452`, artifact `11944786297`, artifact SHA-256 `174a64fe4c55ae438c5466624b63d3bafb92419a5deb9b332af5cd6c98930f09`.

Same 12-cell design as the v1 falsification matrix:

```text
0 improvements
0 regressions
12 inconclusive
```

The cell that falsified v1 changed from a statistically supported loss to:

```text
clustered-hotpool-u10 / FREQ-LRU-REPAIR
PR55 / PR57-v2 = 1.024034×
95% CI = 0.791330–1.325168
INCONCLUSIVE
```

Representative v2 broad point estimates remained close to parity in the repair-heavy profile:

- grid churn / FREQ-LRU-REPAIR: `0.989424×` (CI `0.949366–1.031173`);
- scale-free churn / FREQ-LRU-REPAIR: `1.001802×` (CI `0.877916–1.143171`);
- clustered churn / FREQ-LRU-REPAIR: `1.102011×` (CI `0.934830–1.299089`).

No broad cell supports a general speedup claim. Equally importantly for a specialized rescue mechanism, the broad matrix no longer identifies a statistically supported regression.

## Interpretation

PR57 v2 is a **specialized exact fast path**, not a new universal controller. It is designed to be nearly invisible when repairs are small and to remove catastrophic unnecessary work when a large selected subtree is still certified by an alternate root witness.

The targeted multipliers must not be generalized to ordinary workload speedup. Their scientific value is causal: they demonstrate the asymptotic/work-collapse opportunity on the exact condition PR57 detects. The broad matrix establishes only a current no-regression result, not superiority.

## Disposition

**ACCEPT PR57 v2 into the candidate stack, subject to the normal final compatibility and fresh-holdout gates.**

- v1 eager probing: **REFUTED as the default policy**;
- v2 gated rescue: **SURVIVES correctness, causal mechanism, and broad regression testing**;
- broad performance superiority from PR57 alone: **UNESTABLISHED**.

## Claim boundary

PR57 does not establish practical or theoretical SOTA. A defensible practical SOTA claim still requires a frozen combined candidate, fresh held-out workload evidence, explicit persistent-memory parity, and DIRECT-qualified external comparators under the frozen comparator contract.
