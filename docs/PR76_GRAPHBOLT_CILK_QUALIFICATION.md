# PR76 — GraphBolt historical Cilk Plus toolchain and semantic qualification

## Purpose

Attack the remaining GraphBolt comparator-fidelity concern in the ADES evidence chain.

PR69 semantically qualified GraphBolt/DZiG's upstream `OPENMP` branch and PR70/72/74 used that qualified execution mode for performance. GraphBolt's upstream Makefile, however, foregrounds the historical Cilk Plus build:

`g++ -std=c++14 -fcilkplus -lcilkrts -g -O3 -DCILK ...`

PR76 therefore attempts to recover the historical Cilk substrate in a pinned legacy compiler container and re-run semantic qualification before any Cilk performance comparison is admitted.

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

Any additional source modification invalidates this qualification branch.

## Stage A — legacy Cilk substrate discovery

Stage A uses the Docker Official Image tag:

`gcc:7.5.0`

The tag is used **only to discover and test** the exact legacy image. Stage A must:

1. pull `gcc:7.5.0`;
2. record the full immutable `RepoDigest` returned by Docker;
3. record container OS release and `g++ --version`;
4. compile a standalone Cilk Plus probe with `-std=c++14 -fcilkplus -lcilkrts`;
5. require `<cilk/cilk.h>` and `<cilk/cilk_api.h>` to compile;
6. execute a `cilk_for` computation and require the exact arithmetic result;
7. record the runtime worker count and linked libraries;
8. upload the probe, binary metadata, and digest as evidence.

A Stage-A success has disposition:

`CILK_TOOLCHAIN_DISCOVERED_NOT_SEMANTICALLY_QUALIFIED`

It does **not** qualify GraphBolt and cannot support performance work.

A Stage-A failure is preserved as:

`CILK_TOOLCHAIN_DISCOVERY_FAILED`

and must not be relabeled as evidence against GraphBolt's algorithm.

## Stage B — canonical semantic qualification

Stage B may be created only if Stage A succeeds. It must replace the tag with the exact full image digest discovered by Stage A and then:

1. fresh-pull the digest-pinned image;
2. clone exact GraphBolt commit `7c2792cc77d59b9722288ded8c3c62f6a9a87fc7` on the host;
3. verify upstream source hashes and clean status;
4. apply only `-include cstdint` plus the PR69 one-line missing-return repair;
5. provide pinned mimalloc v1.6.0 built compatibly inside the same legacy container, because GraphBolt documents mimalloc as required for correct/efficient operation;
6. build weighted SSSP with the upstream historical Cilk flags, not OpenMP;
7. run the unchanged PR69 9-case semantic matrix with a fixed worker count;
8. require 9/9 exact cases and every preregistered fresh-Dijkstra check;
9. preserve all source/build/toolchain hashes and the exact container digest.

Only Stage B may yield:

`QUALIFIED_HISTORICAL_CILK_DIRECTED_FIXED_SOURCE_WEIGHTED_STREAMING_SSSP_WITH_UINT16_DISTANCE_BOUND`

No Cilk performance experiment is admitted before that exact semantic result.

## Claim boundary

PR76 can establish only reproducibility and semantic correctness of the pinned historical Cilk execution path on the frozen qualification matrix.

It does not establish that Cilk is faster than OpenMP, that ADES beats Cilk GraphBolt, or any universal/SOTA claim.
