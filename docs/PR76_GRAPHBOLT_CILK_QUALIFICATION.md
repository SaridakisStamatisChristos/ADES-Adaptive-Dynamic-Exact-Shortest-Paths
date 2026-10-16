# PR76 — GraphBolt historical Cilk Plus toolchain and semantic qualification

## Purpose

Attack the remaining GraphBolt comparator-fidelity concern in the ADES evidence chain.

PR69 semantically qualified GraphBolt/DZiG's upstream `OPENMP` branch and PR70/72/74 used that qualified execution mode for performance. GraphBolt's upstream Makefile, however, foregrounds the historical Cilk Plus build:

`g++ -std=c++14 -fcilkplus -lcilkrts -g -O3 -DCILK ...`

PR76 therefore recovers the historical Cilk substrate in a pinned legacy compiler container and re-runs semantic qualification before any Cilk performance comparison is admitted.

This PR is **semantic/toolchain qualification only**. It makes no performance claim.

## Frozen GraphBolt identity

- repository: `pdclab/graphbolt`
- commit: `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`
- application: weighted `apps/SSSP.C`
- weighted build flags: `-DEDGELONG -DEDGEDATA`
- semantic matrix: reuse the frozen PR69 harness `tools/run_pr69_graphbolt_qualification.py`
- expected scope if qualified: directed fixed-source positive-integer weighted streaming SSSP with finite distances below 65,535
- logical weight replacement: `delete(old)` then `add(new)`

The only GraphBolt source repair permitted after toolchain recovery is the already-qualified PR69 portability repair:

`return 0;` at the end of the no-argument `traditionalIncrementalComputation()` whose return value is ignored by its caller.

Any additional GraphBolt source modification invalidates this qualification branch.

## Stage A — legacy Cilk substrate discovery

Stage A used the Docker Official Image tag `gcc:7.5.0` only to discover and test the exact legacy image. It required a real `cilk_for` probe, exact result, worker/runtime evidence, linked libraries, compiler identity, and immutable image identity.

Canonical Stage-A evidence:

- head: `a9725e679c876de25767b820de5e8da7a5571459`
- run: `38843716006`
- job: `116789353550`
- artifact: `12021922178`
- artifact SHA-256: `8222e6491790ff4314468a4ef0c7c08c896b1a080d5ba39be8ae7aed6a178c32`
- discovered immutable image: `gcc@sha256:1938a2199146f37d7c5b697a66d4379b75e3cd6ca5e08380c6803ab25e0ba9a4`
- disposition: `CILK_TOOLCHAIN_DISCOVERED_NOT_SEMANTICALLY_QUALIFIED`

Stage A alone did not qualify GraphBolt and was not performance evidence.

## Stage B — canonical semantic qualification

Stage B replaces the mutable image tag with the exact Stage-A digest and then:

1. fresh-pulls the digest-pinned image;
2. clones exact GraphBolt commit `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7`;
3. verifies upstream source hashes and clean status;
4. applies only `-include cstdint` plus the PR69 one-line missing-return repair;
5. provides pinned mimalloc v1.6.0 (`04f1c3b1e23677ce03bd16137e73089abd552175`), because GraphBolt documents mimalloc as required;
6. builds weighted SSSP with the historical Cilk flags, not OpenMP;
7. runs the unchanged PR69 9-case semantic matrix with `CILK_NWORKERS=4`;
8. requires 9/9 exact cases and all 729 preregistered fresh-Dijkstra checks;
9. preserves source/build/toolchain hashes and the exact container digest.

### Preserved Stage-B preflight failure

The first Stage-B build on head `9baec6e5876f36da24ea0ee8401a6250d90d4318` failed before GraphBolt compilation because mimalloc v1.6.0 was compiled with strict `-std=c11`, while that pinned source uses GNU inline `asm`. This is classified as a build-language-mode/environment failure, not a GraphBolt semantic counterexample.

The minimal source-preserving repair changed only the allocator language mode to `-std=gnu11`. GraphBolt SHA, mimalloc SHA, Cilk image digest, PR69 portability patch, worker count, semantic matrix, timeout, and admission rule remained frozen.

### Canonical Stage-B result

Accepted head:

`e5e06aef3b6d35aca40c98183553755dd134f3fe`

Canonical execution:

- workflow: `ADES PR76 GraphBolt historical Cilk qualification`
- run: `38854118754`
- job: `116821640368`
- artifact: `12026410989`
- artifact ZIP SHA-256: `56fd8c8cadc3ed41414f3b1bdbefe62a8aebd15f81c261ef2742720b0acc0166`
- Cilk workers: 4
- planned cases: 9
- exact cases: 9/9
- exact checks: 729/729
- first failure: none
- full matrix executed: yes
- performance executed: no

Qualification:

`QUALIFIED_HISTORICAL_CILK_DIRECTED_FIXED_SOURCE_WEIGHTED_STREAMING_SSSP_WITH_UINT16_DISTANCE_BOUND`

This establishes reproducibility and semantic correctness of the digest-pinned historical Cilk Plus execution path for the frozen PR69 matrix, with the already-qualified one-line missing-return portability repair.

## Claim boundary

PR76 establishes that the historical Cilk Plus GraphBolt execution substrate can be reproduced and that the qualified patched GraphBolt weighted SSSP produces exact results on the frozen 9-case / 729-check matrix.

It does **not** establish that Cilk is faster than OpenMP, that ADES beats historical-Cilk GraphBolt, or any universal/SOTA claim. Those require a separate preregistered performance experiment using this exact qualified substrate.
