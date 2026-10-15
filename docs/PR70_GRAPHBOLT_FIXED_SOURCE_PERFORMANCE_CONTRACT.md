# PR70 — GraphBolt fixed-source dynamic SSSP performance contract

Status: **PREREGISTERED BEFORE COMPARATIVE TIMINGS**

This document freezes the first performance experiment admitted by PR69. It is deliberately narrow. A passing result may support superiority of the **ADES resident fixed-source maintenance core** on this tested slice; it must not be promoted to universal ADES superiority, arbitrary-source superiority, all-graph SOTA, or historical GraphBolt/Cilk superiority.

## 1. Comparator identity

GraphBolt / DZiG:

- repository: `pdclab/graphbolt`
- commit: `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- application: weighted `apps/SSSP.C`
- required allocator: mimalloc v1.6.0 commit `04f1c3b1e23677ce03bd16137e73089abd552175`
- parallel path: upstream `OPENMP` branch, primary performance setting `OMP_NUM_THREADS=4`
- build: `-O3 -DOPENMP -fopenmp -DEDGELONG -DEDGEDATA`
- source-preserving compatibility include: compiler `-include cstdint`
- portability patch: `kickstarter-missing-return-v1`, exactly one added `return 0;` at the end of the no-argument `KickStarterEngine::traditionalIncrementalComputation()`.

The portability patch is required by PR69 causal evidence. It changes no shortest-path, graph-update, scheduling, or relaxation logic. Any additional GraphBolt source repair invalidates the performance gate and requires a new semantic qualification first.

ADES:

- frozen code lineage: PR69 head `e31f6b6c8b3072941276e0bbf55cacdf4970fe7d`
- compared component: one materialized `ades::SSSPState` maintained by the same `repair_decrease` / `repair_increase` logic and exact Dijkstra fallback used by resident ADES entries.
- the experiment does **not** time arbitrary-source admission, eviction, source caching, or cold-query policy. This is intentional: GraphBolt's qualified interface is fixed-source state maintenance.

## 2. Logical task

Maintain exact single-source shortest-path distances on a directed graph under online **weight replacement**.

For each logical replacement `(edge, old_weight -> new_weight)`:

- ADES performs one native weight update and its normal resident repair/rebuild path.
- GraphBolt has no native weight-replacement operation in this application, so the same logical replacement is represented by the qualified upstream mapping: `delete(u,v,old_weight)` followed by `add(u,v,new_weight)`.
- GraphBolt is charged for both update batches. This is interface adaptation required to implement the same logical task, not an extra synthetic workload.

No future update is visible to either implementation.

## 3. Frozen holdout matrix

Independent graph seeds:

`7001, 7003, 7009, 7013`

Per seed:

- vertices: `4096`
- initial directed edges: `32768`
- topology: deterministic sparse non-road directed graph consisting of a directed ring for reachability plus unique pseudorandom extra arcs
- edge weights: positive integers in `[1, 30]`
- fixed source: `(seed mod 4096)`
- logical weight replacements: `128`
- replacement mode: strict alternating increase/decrease where feasible
- replacement magnitude: integer delta in `[1, 7]`
- repeated endpoints are allowed only if each replacement records and uses the current weight at that time
- no topology insertion/deletion is part of the logical ADES task.

Hard semantic gate: after graph construction and after every logical replacement, every finite oracle distance must be `< 65535`, matching GraphBolt's qualified `uint16_t` application boundary. A seed violating this rule is a failed preregistered cell; it must not be silently regenerated with a new seed after timing results are seen.

## 4. Correctness side gate

Performance is inadmissible unless all four seeds are exact.

For every seed:

1. Generate one frozen graph and update stream and hash both.
2. Run a **non-timed correctness pass** for GraphBolt with output enabled.
3. Check GraphBolt distances after initial computation and after the add phase completing every logical replacement against fresh Dijkstra from the same current graph.
4. Run an ADES correctness pass using the identical graph, source, and logical replacements; compare its maintained state after every replacement against fresh Dijkstra.
5. Any mismatch, missing output, process failure, overflow-bound violation, or changed update stream rejects that cell.

Intermediate GraphBolt deletion states are not compared to ADES because they are an adapter implementation detail of one logical weight replacement; PR69 already semantically qualified both deletion and addition phases.

## 5. Timed region

Primary timing is **algorithm time**, not file generation or correctness-output serialization.

GraphBolt:

- omit `-outputFile` during timed runs;
- parse `Initial graph processing : <seconds>` as initialization time;
- parse every `Finished batch : <seconds>` line;
- maintenance time = sum of all 256 batch times for 128 logical replacements;
- total algorithm time = initialization + maintenance.

ADES:

- parse the frozen graph/update files before entering timed regions;
- initialization time = `ades::dijkstra(graph, source)` only;
- maintenance time = sum of graph weight update + repair/controller-equivalent resident maintenance + exact Dijkstra fallback where required;
- total algorithm time = initialization + maintenance.

The ADES runner must expose rebuild counts and repair-result counts so a fast result cannot hide skipped work.

Secondary disclosure fields, not the primary claim metric:

- process wall time around each executable;
- peak RSS when available;
- GraphBolt per-batch count;
- ADES rebuild/repair/filter counts.

Input parsing and process startup are therefore reported separately and are not mixed into the primary algorithm ratio.

## 6. Repetition and execution order

For each seed, execute two measured paired repetitions:

- repetition 0: GraphBolt then ADES
- repetition 1: ADES then GraphBolt

The same frozen files are used in both repetitions. No workload regeneration is permitted between implementations or repetitions.

GraphBolt primary performance runs use `OMP_NUM_THREADS=4`. ADES is the repository's current single-threaded implementation. Giving GraphBolt its admitted parallel runtime is intentional; a result favoring ADES therefore cannot be attributed to artificially forcing GraphBolt single-threaded.

## 7. Statistics

For each paired repetition compute:

- `maintenance_ratio = graphbolt_maintenance / ades_maintenance`
- `total_ratio = graphbolt_total / ades_total`

Aggregate in two stages:

1. geometric mean of the two repetitions within each seed;
2. geometric mean across the four independent seed-level ratios.

Compute deterministic 95% bootstrap confidence intervals by resampling the four seed-level ratios with replacement using bootstrap RNG seed `70070` and at least `20,000` resamples.

## 8. Admission / falsification rule

The experiment supports the narrow claim **ADES resident fixed-source maintenance core is faster on the PR70 tested slice** only if all of the following hold:

- correctness: 4/4 seeds exact for both implementations;
- distance-bound gate: 4/4 seeds remain within GraphBolt's qualified range;
- execution: all 8 measured pairs complete without timeout or malformed timing output;
- maintenance geometric-mean ratio > 1.0 and its 95% bootstrap lower bound > 1.0;
- total geometric-mean ratio > 1.0 and its 95% bootstrap lower bound > 1.0;
- no independent seed has a seed-level maintenance ratio < 1.0.

If any condition fails, the preferred superiority hypothesis is falsified for this slice. Preserve the result. Do not tune seeds, graph scale, update count, weight range, thread count, or repair budget after seeing comparative timings.

## 9. Claim boundaries

Even a strong pass does not establish:

- universal ADES > GraphBolt;
- arbitrary-source superiority;
- superiority on external real-world topology;
- superiority over GraphBolt's historical Cilk build;
- equal-memory superiority;
- all-graph or theoretical SOTA.

A strong pass would justify only the frozen fixed-source dynamic-maintenance slice and would motivate a later external-topology replication.

## 10. Evidence requirements

The canonical artifact must contain:

- exact ADES head SHA;
- exact GraphBolt and mimalloc SHAs;
- patch diff and SHA-256;
- compiler/runtime versions and GraphBolt linkage;
- graph and stream hashes for every seed;
- correctness summary and first failure, if any;
- raw measured timing rows;
- seed-level ratios;
- bootstrap summary;
- peak-RSS/process-wall disclosures where available;
- final disposition string that cannot exceed the claim boundary above.
