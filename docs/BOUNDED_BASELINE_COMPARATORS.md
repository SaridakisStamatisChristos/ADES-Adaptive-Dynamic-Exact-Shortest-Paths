# PR36: Bounded-residency comparator methodology

The phase runner supports **B2L** (full-rebuild LRU) and **B3L** (local-repair LRU). Original B2 and B3 are unchanged. The existing optional positional `cap` argument is the maximum number of resident source SSSP states for B2L/B3L, as for the ADES B4 resident-source cap. A zero cap is rejected for B2L/B3L.

LRU is deterministic: every query hit makes that source most recently used; on a miss, a complete Dijkstra state is computed and the least recently queried resident source is evicted when full. Updates apply to all currently resident states using the corresponding B2 or B3 update strategy. Evicted sources are recomputed upon a subsequent query. The independent oracle continues to validate every answer.

B2L/B3L emit the existing CSV schema, with `cold_queries` counting cache misses, `resident_queries` counting hits, and `evictions` counting evictions. Their additional resident counts appear in stderr. They are not part of the legacy `ALL` set: request them explicitly to avoid silently altering earlier experimental definitions.

**Memory fairness limitation:** A shared `cap` gives a common maximum number of resident source states, **not** a strict equal RSS budget. B4 also retains controller and admission metadata and can allocate temporary states; all variants have graph and oracle allocations. Compare measured peak RSS in addition to runtime. The vector element payload lower bound is 44 bytes per vertex per resident source, excluding allocator, container, graph and temporary overhead. Equal *byte* budgets require further calibration, not just matching caps.

Example (small smoke workload):

```sh
./build/ades_phase benchmarks/data/USA-road-d.NY.gr.gz B2L uniform 10 0 1 250 7 8
./build/ades_phase benchmarks/data/USA-road-d.NY.gr.gz B3L uniform 10 0 1 250 7 8
```

This PR does not claim production memory safety or comparative state of the art. Use matched traces, seeds, caps, hardware, correctness checks, repetition and RSS measurement before interpreting performance.
