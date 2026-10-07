# Evidence ledger

This ledger records the current evidence state of the implemented ADES architecture. It distinguishes implementation/experimental evidence from novelty or asymptotic claims.

| Component | State | Repository evidence |
|---|---|---|
| Dijkstra oracle | EMPIRICALLY_VALIDATED | deterministic differential tests; benchmark oracle generation |
| Bidirectional cold search | EMPIRICALLY_VALIDATED | differential tests; NY smoke; B1 benchmark cells |
| Cost-aware admission | EMPIRICALLY_VALIDATED on current NY workloads | B4 remains cold on cheap local/clustered/moving workloads and selectively promotes on expensive cross-region demand; see `evidence/ny-2026-10-07/` |
| Decrease local repair | EMPIRICALLY_VALIDATED | implemented in `src/dynamic_repair.cpp`; adversarial/differential tests against fresh Dijkstra |
| Increase selected-subtree repair | EMPIRICALLY_VALIDATED | implemented boundary-seeded restricted repair with persistent SPT links, early-abort discovery, and differential tests |
| Tight non-parent increase handling | CONSERVATIVE_EXACT | current implementation rebuilds rather than assuming selected-parent structure captures all equal-length alternatives |
| Read-only early-abort discovery | EMPIRICALLY_VALIDATED | regression tests protect semantic state before rebuild fallback |
| Adaptive repair/rebuild controller | PARTIALLY_REFUTED AS ECONOMIC POLICY | controller correctly aborts large affected regions, but NY catastrophic-cut evidence shows abort+rebuild can be slower than completing fixed repair |
| Memory economics of adaptive residency | EMPIRICALLY_SUPPORTED | archived NY matrix shows B4 generally near cold-baseline RSS while unconditional resident baselines consume much more memory |
| Formal exactness specification | REPOSITORY_PROOF_SKETCH | `docs/ALGORITHM.md`; not machine-checked formal verification |
| Worst-case asymptotic improvement over Dijkstra | NOT CLAIMED | local repair can degenerate to full-SSSP scale |
| Universal fastest exact dynamic shortest paths | NOT ESTABLISHED | unsupported by current evidence |
| Novelty | UNESTABLISHED | requires dedicated literature preflight after algorithm freeze |

## Evidence boundary

The permanent NY evidence package is archived at `evidence/ny-2026-10-07/` with raw CSVs, metadata, provenance, summary, and integrity hashes.

That package is mixed-provenance engineering evidence: most successful cells come from the base definitive run, while the previously failing travel-time/local cell and travel-time controller evidence were recovered after the `SpatialGrid` initialization-order UB fix. Publication-grade evidence should rerun the complete matrix on one fixed commit, across multiple independent workload seeds and preferably longer traces.

The five repetitions in the archived matrix are timing repetitions of the same deterministic trace, not independent workload samples. Median and p95 are therefore descriptive statistics rather than confidence intervals.
