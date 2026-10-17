# B2/B3 memory preflight (PR35)

The phase runner now counts **distinct query sources** in the exact generated trace before computing the reference oracle or running a baseline. It reports the number of vertices, distinct sources and a **lower bound** on resident SSSP vector payload: `distinct_sources * vertices * 44` bytes, derived from the six vector element types in `SSSPState`.

When `ADES_MAX_RESIDENT_BYTES` is set to a positive decimal byte count, B2, B3 and ALL return **exit code 4** before benchmark execution if this lower bound exceeds the budget. Unset means legacy unlimited behavior. B1 and B4 are not gated by this variable. The B2-only Actions diagnostic sets the bound to 4 GiB and prints the preflight message into its logs.

**Caution:** The estimate excludes graph copies, vector capacities, allocator/hash-map overhead, Dijkstra queues, trace generation, oracle allocations and temporary rebuild states. Passing the guard is **not** a guarantee against OOM; it only rejects provably over-budget resident payloads. The byte count is platform-type dependent (64-bit Distance/int64, 32-bit uint32) and is not a measured RSS. This guard does not evict sources, modify B2/B3 semantics, or make equal-budget comparisons possible.

Prior observations: B2 100 queries 1,229,072 KiB RSS; 250 queries 2,933,884 KiB RSS; isolated 2,000-query run interrupted (cause unconfirmed). These are diagnostic observations, not publishable crossover evidence.

For future fair comparisons, create separately named bounded-residency baselines (planned PR36) and preserve B2/B3 unchanged. Do not interpret a skipped or rejected run as a performance sample.
