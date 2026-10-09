# PR49 — Publication Viability Pilot

## Purpose

PR49 is the first campaign stage whose primary output is a scientific decision rather than another mechanism. It runs a bounded, reproducible pilot over multiple graph classes, locality regimes, update rates, byte budgets, seeds, repetitions, bounded comparators, and the PR45 policy ladder.

The allowed outcomes are:

```text
GO
REFRAME
NO-GO
```

The decision rule is frozen before pilot measurements are inspected.

## Scope

This is a **pilot**, not the definitive manuscript benchmark. It is intended to answer whether the campaign is worth expanding and, if so, under what claim framing.

**Claims addressed:** `C1`–`C9` and `C-SOTA` viability.

## Frozen matrix

### Graphs

Three materially different graph classes are used:

1. `ny-road-distance` — committed DIMACS New York road graph;
2. `syn-grid-224x224-v1` — PR47 deterministic grid;
3. `syn-scale-free-50000-m4-v1` — PR47 deterministic scale-free graph.

Each graph is identified by SHA-256.

### Locality regimes

Three Workload Generator v2 source regimes are used:

```text
uniform       locality_percent = 0
single-hot    locality_percent = 95
churn         locality_percent = 100
```

They deliberately span low reuse, strong reuse, and high-turnover source behavior.

### Update pressure

Two frozen update intervals are used:

```text
5   = approximately one update per five queries
50  = approximately one update per fifty queries
```

Updates use `strict-alternating` direction and `medium` perturbation magnitude under Workload Generator v2.

### Byte budgets

```text
16 MiB
64 MiB
```

The same PR43 logical persistent-state byte budget is applied to B2L, B3L, and every resident-capable ADES ablation. Source-count ceilings are made nonbinding.

### Independent samples and repetitions

```text
seeds = 7, 17, 29
repetitions per seed = 2
```

Seeds are the independent workload samples. Repetitions measure runtime repeatability on the same trace and are **not** treated as independent workloads in confidence intervals.

### Queries

The default pilot uses 60 queries per trace. This is intentionally smaller than a definitive benchmark and is interpreted only as a viability signal.

## Profiles

Every matched cell executes:

```text
B2L
B3L
COLD
FREQ-LRU-REBUILD
FREQ-LRU-REPAIR
WORK-LRU-REPAIR
WORK-DEBT-REPAIR
ADES
```

All ADES policy profiles use the PR45 same-engine implementation.

## Process isolation

Each profile executes in its own process.

The exact oracle bundle is built in a **separate process before comparator timing**. Comparator processes read only the compact oracle answers. Consequently:

- oracle Dijkstra construction is excluded from algorithm timing;
- oracle Dijkstra allocation/RSS cannot contaminate comparator process peak RSS;
- every query is still validated exactly against the same trace-specific oracle bundle.

Profile execution order rotates across trace/budget/repetition combinations to reduce systematic order bias.

## Trace generation

Every trace is produced by Workload Generator v2 and archived with its metadata.

Each profile independently reloads the canonical trace and requires:

- valid evolving `old_weight` state;
- matching trace SHA-256;
- exact oracle agreement;
- matching declared byte budget;
- peak accounted persistent state not exceeding budget.

## Statistics

For each regime and named comparator, PR49 computes paired:

```text
speedup = comparator_algorithm_time / ADES_algorithm_time
```

Thus a value greater than `1` favors ADES.

The two repetitions are averaged in log space within each seed. The confidence interval is then computed across independent seed-level log means and exponentiated back to the speedup scale.

With exactly three seeds, the pilot uses the two-sided 95% Student-t critical value for `df = 2`:

```text
4.302652729911275
```

This is intentionally conservative. The definitive campaign may freeze a stronger resampling/statistical design later.

Paired win/tie/loss uses a ±1% tie band.

## Decision rule

The PR49 decision is computed mechanically by `tools/run_publication_pilot.py`.

### GO

`GO` requires both:

1. at least one PR48 `DIRECT` external comparator; and
2. at least one material controlled ADES win against `FREQ-LRU-REPAIR`, defined in the pilot as geometric-mean speedup `>= 1.10` with 95% CI lower bound `> 1.0`.

PR48 currently has zero DIRECT external comparators. Therefore the present pilot **cannot legitimately produce GO** unless comparator qualification changes before the run.

### REFRAME

If GO is blocked, choose `REFRAME` when the pilot still shows a nontrivial scientific signal, operationalized as at least one of:

- a material ADES win against `FREQ-LRU-REPAIR` under the rule above;
- ADES is the fastest median profile in at least one frozen regime;
- more than one profile is fastest across the tested regime space, demonstrating a crossover/phase-structure signal.

### NO-GO

Choose `NO-GO` if GO is unavailable and none of the internal-signal conditions above occurs.

This means a simple strategy can kill the SOTA direction if the pilot shows no material adaptive advantage and no meaningful crossover structure.

## Required outputs

The pilot package contains:

```text
measurements.csv
summary.csv
confidence.csv
ANALYSIS.md
decision.json
manifest.json
SHA256SUMS
traces/
oracles/
raw/
```

`measurements.csv` is the canonical raw numeric evidence table. Losses and contradictory cells are never filtered.

## Permanent archive

The full pilot workflow uploads the complete result package as a GitHub Actions artifact. A compact package suitable for permanent repository preservation consists of the required top-level analysis files plus trace and oracle bundles; deterministic PR47 generated graph files need not be duplicated because their generator specification and hashes are already pinned.

## Interpretation constraint from PR48

A `REFRAME` result can justify a strong paper about adaptive residency, crossover structure, workload dependence, or phase behavior.

It **cannot** be rewritten as a scoped external-comparator SOTA claim while the PR48 DIRECT comparator set is empty.
