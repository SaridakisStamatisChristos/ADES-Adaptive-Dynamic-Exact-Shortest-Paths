# PR77 — preregistered historical-Cilk GraphBolt vs ADES fixed-source performance gate

## Purpose

Run the smallest discriminating performance experiment admitted by PR76.

PR70 showed a strong ADES win over the PR69-qualified GraphBolt OpenMP4 comparator on a frozen synthetic directed fixed-source slice. PR76 then recovered and semantically qualified GraphBolt's historical Cilk Plus execution path. PR77 changes only the GraphBolt parallel/runtime substrate from the PR70 OpenMP4 path to the PR76-qualified historical Cilk4 path while reusing the PR70 workload family, ADES runner, correctness requirements, repetition structure, timing definitions, and statistical decision rule.

This contract is committed before any PR77 comparative timing result is observed.

## Frozen lineages

ADES candidate/core lineage:

`cf7cbc3ed7d9d37ad673a540485a1eee781d47fe`

ADES fixed-source runner:

reuse unchanged `tools/pr70_ades_fixed_source.cpp` inherited through the evidence stack.

GraphBolt/DZiG:

- repository: `pdclab/graphbolt`
- commit: `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- weighted application: `apps/SSSP.C`
- mimalloc: `04f1c3b1e23677ce03bd16137e73089abd552175` (v1.6.0)
- historical Cilk image: `gcc@sha256:1938a2199146f37d7c5b697a66d4379b75e3cd6ca5e08380c6803ab25e0ba9a4`
- build mode: `-std=c++14 -fcilkplus -O3 -DCILK -DEDGELONG -DEDGEDATA -include cstdint`
- workers: `CILK_NWORKERS=4`
- allocator build mode: GNU C11 (`-std=gnu11`), as established by PR76
- only GraphBolt source repair: `kickstarter-missing-return-v1`, the PR69-qualified one-line missing-return portability repair

Any additional GraphBolt source modification invalidates PR77 and requires fresh semantic qualification.

PR76 semantic prerequisite:

- accepted execution head: `e5e06aef3b6d35aca40c98183553755dd134f3fe`
- run: `38854118754`
- job: `116821640368`
- artifact: `12026410989`
- artifact ZIP SHA-256: `56fd8c8cadc3ed41414f3b1bdbefe62a8aebd15f81c261ef2742720b0acc0166`
- result: 9/9 exact cases, 729/729 checks

## Frozen workload

Reuse the PR70 workload generator and parameters exactly:

- independent seeds: `7001, 7003, 7009, 7013`
- vertices: 4,096
- directed edges: 32,768
- topology: deterministic ring plus unique pseudorandom directed non-road arcs
- initial positive integer weights: `[1,30]`
- fixed source: `seed mod 4096`
- logical weight replacements: 128 per seed
- replacement schedule: strict alternating decrease/increase where feasible
- update delta: integer `[1,7]`
- GraphBolt encoding of one logical replacement: `delete(old)` then `add(new)`, therefore 256 GraphBolt stream batches per seed
- hard finite-distance gate: every oracle state must remain `< 65535`
- measured repetitions: two per seed
- order balancing: repetition 0 `GraphBolt -> ADES`; repetition 1 `ADES -> GraphBolt`

No seed, graph scale, edge count, source rule, update sequence, weight range, repetition count, worker count, or decision threshold may be changed after comparative timing results are observed.

## Correctness-before-performance gate

Before any measured timing pair for a seed:

1. GraphBolt historical-Cilk runs a non-timed full-output pass.
2. Fresh Dijkstra checks initialization and every completed logical replacement.
3. ADES runs its inherited exact pass and is checked against fresh Dijkstra for the same logical states.
4. Every finite oracle distance must remain below 65,535.
5. Any mismatch, missing output, nonzero exit, timeout, or bound violation makes performance for that seed inadmissible.

Performance is admitted only if all four seeds pass correctness for both implementations.

## Timing contract

Primary metrics use each implementation's internal algorithm timer, matching PR70's contract.

GraphBolt historical Cilk:

- initialization = reported `Initial graph processing`
- maintenance = sum of all 256 reported `Finished batch` times
- total = initialization + maintenance

ADES:

- initialization = fixed-source initial Dijkstra in the PR70 resident-state runner
- maintenance = all graph weight replacement plus repair/controller-equivalent work and exact fallback work performed by that runner
- total = initialization + maintenance

Process wall time is recorded separately. GraphBolt's Docker process-launch overhead is not included in the primary internal-algorithm metric and must not be silently compared with ADES internal time.

## Statistical rule

For both maintenance and total time:

1. compute GraphBolt/ADES ratio for each measured pair;
2. geometric-mean the two repetitions within each seed;
3. geometric-mean the four independent seed-level ratios;
4. compute the deterministic 95% bootstrap interval over seed-level ratios using 20,000 resamples and RNG seed `70070`.

The preferred narrow hypothesis is supported only if:

- 4/4 seeds are exact for both implementations;
- all finite-distance gates pass;
- all 8 measured pairs complete;
- maintenance geometric mean > 1 and bootstrap 95% lower bound > 1;
- total geometric mean > 1 and bootstrap 95% lower bound > 1;
- no seed-level maintenance ratio is < 1.

Otherwise the completed experiment is retained as a falsification or mixed result. CI must be outcome-neutral: green means the preregistered experiment completed validly, not that ADES won.

## Frozen dispositions

If the preferred hypothesis passes:

`SUPPORTS_ADES_RESIDENT_FIXED_SOURCE_CORE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR77_TESTED_SLICE`

If correctness/bounds fail:

`REJECTED_PR77_CORRECTNESS_OR_DISTANCE_BOUND`

If the experiment completes but the preferred performance rule fails:

`FALSIFIES_PREFERRED_PR77_CILK_SUPERIORITY_HYPOTHESIS_ON_TESTED_SLICE`

## Claim boundary

Even a support result establishes only that the ADES resident fixed-source dynamic SSSP core outperformed the PR76-qualified historical-Cilk GraphBolt comparator on the frozen PR70-derived synthetic directed slice under this timing contract.

It does not establish arbitrary-source superiority, equal-memory superiority, universal superiority, theoretical SOTA, all-topology practical SOTA, or superiority on real external topologies under historical Cilk. If PR77 supports the hypothesis, the next admissible generalization should reuse one already-frozen external real topology rather than generate a new synthetic slice.
