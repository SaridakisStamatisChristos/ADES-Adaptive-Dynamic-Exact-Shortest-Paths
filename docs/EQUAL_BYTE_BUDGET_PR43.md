# PR43 — Equal-byte persistent-state budget engine

## Claims addressed

```text
claim_ids: [C1, C2, C3]
```

PR43 establishes the benchmark mechanism required by publication-contract claim **C3**: ADES and the bounded resident comparators must obey the same declared algorithm-owned persistent-state byte budget. Exactness (`C1`) and matched trace identity (`C2`) remain mandatory gates for every measured cell.

## Why source-count caps are insufficient

B2L, B3L and B4 can all be limited to the same number of resident sources, but their persistent state is not identical. B2L/B3L maintain LRU metadata; B4 additionally maintains admission/probation, cooldown, residency scoring and repair-controller state. A shared source count therefore does not prove a shared memory budget.

PR43 introduces a versioned logical accounting model and hard enforcement. The fairness control is **not** process RSS. RSS remains separately measured because it includes allocator behavior, common graph storage, code/runtime pages, temporary workspaces and other implementation effects that are not the publication budget variable.

## Accounting model v1

The machine-readable version is:

```text
accounting_version = 1
```

The common metric is:

```text
accounted_algorithm_state_bytes
```

### Included

For every resident exact SSSP state:

- source id and repair epoch;
- distance vector;
- selected-parent edge vector;
- first-child, next-sibling and previous-sibling vectors;
- repair-mark vector.

The semantic SSSP payload is therefore:

```text
8 + |V| * (sizeof(Distance) + 4*sizeof(int64_t) + sizeof(uint32_t))
```

On the current 64-bit representation this is `8 + 44*|V|` bytes.

B2L/B3L additionally account for their logical map source key and LRU source id per resident state.

B4 additionally accounts for:

- resident map source key;
- hit, last-query and update-debt counters;
- persistent repair-controller model state;
- probation source/query/edge-scan records;
- cooldown source/expiry records.

### Excluded

The logical publication budget deliberately excludes:

- graph storage, which is common input state rather than adaptive residency state;
- STL allocator, bucket, node and control-block implementation overhead;
- vector spare capacity beyond semantic vector size;
- temporary Dijkstra/repair/query workspaces;
- benchmark oracle state;
- telemetry/statistics and immutable configuration;
- executable/runtime pages and unrelated process memory.

Those exclusions do **not** make physical memory invisible. `/usr/bin/time` peak RSS remains independently recorded and must accompany publication-facing memory results.

## Hard enforcement

A nonzero environment variable activates the common budget:

```sh
ADES_PERSISTENT_STATE_BUDGET_BYTES=<bytes>
```

### B2L/B3L

On a miss, the newly computed SSSP state is temporary until admission. Before it becomes persistent, LRU residents are evicted until the candidate fits the declared byte budget. If one candidate state cannot fit by itself, the query remains exact but the state is not retained.

### B4

B4 accounts both resident SSSP state and nonresident policy metadata.

Before a resident candidate is admitted:

1. stale cooldown records are removed;
2. if necessary, low-value probation records and then earliest-expiring cooldown records are deterministically pruned so the candidate can fit in principle;
3. all required resident evictions are planned without mutating the cache;
4. the existing admission-hysteresis rule must approve every planned victim;
5. the SSSP state is computed as temporary work;
6. approved victims are removed and the candidate is inserted;
7. cooldown metadata is retained only when it also fits the hard budget.

If policy metadata cannot fit, it may be dropped; exact query/update semantics are never weakened to preserve metadata.

At every persistent-state checkpoint:

```text
accounted_algorithm_state_bytes <= persistent_state_budget_bytes
peak_accounted_algorithm_state_bytes <= persistent_state_budget_bytes
```

A violation is a hard failure.

## Source-count behavior in equal-byte mode

When `ADES_PERSISTENT_STATE_BUDGET_BYTES` is active for B2L, B3L or B4, `ades_phase` raises the effective source-count ceiling to `|V|`. This makes source count nonbinding so the byte budget—not an inherited source cap—controls residency.

Legacy source-cap experiments remain available when the environment variable is absent. They are engineering diagnostics and are explicitly not equal-byte publication evidence.

## Phase-runner output

PR43 adds the following fields to phase output:

```text
persistent_state_budget_bytes
accounted_algorithm_state_bytes
peak_accounted_algorithm_state_bytes
accounting_version
```

They are emitted alongside PR42 trace identity and operation counts.

## Equal-byte driver

Use:

```sh
python3 tools/equal_byte_crossover.py \
  --graph benchmarks/data/USA-road-d.NY.gr.gz \
  --budgets-bytes 67108864 \
  --queries 100 \
  --repeats 2 \
  --seeds 7 17 \
  --families single-hot hot-pool \
  --update-every 5 10 \
  --update-mode alternating
```

For each matched cell the driver requires B2L, B3L and B4 to agree on:

- configured byte budget;
- accounting-model version;
- SHA-256 trace identity;
- query/update and update-direction counts.

It separately verifies that each algorithm's reported peak accounted persistent state never exceeds the common budget. Raw stdout/stderr, process RSS, timing, manifest and summary are retained.

The generated manifest declares:

```text
claim_ids: [C1, C2, C3]
```

## CI acceptance

`.github/workflows/equal-byte-budget.yml` performs:

- full build and CTest;
- PR43 memory-budget acceptance tests;
- Python syntax validation;
- an NY DIMACS equal-byte smoke cell using B2L/B3L/B4;
- alternating increase/decrease updates;
- SHA-256 trace parity;
- identical byte-budget/accounting-version checks;
- hard peak-accounted-byte enforcement.

## Scientific interpretation

PR43 establishes the **fairness mechanism**, not a performance result. Existing pre-PR43 matched-source-cap evidence remains useful engineering evidence but does not satisfy publication-contract C3. Runtime/Pareto claims must be re-earned under equal-byte runs after this PR is merged.
