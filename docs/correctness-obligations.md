# Correctness obligations

Core invariants: stable edge IDs; nonnegative weights; overflow-safe relaxation; exact query answers; controller decisions may change performance but never correctness.

The current update path is intentionally conservative: an update that may invalidate a resident SSSP triggers a fresh exact Dijkstra rebuild. Safe filters are (1) an increased edge that was not tight under the old distances and (2) a decreased edge whose new relaxation cannot improve its head.

Before local repair replaces the fallback, the repository must prove/test decrease propagation, increase affected-region reconstruction with ties, read-only discovery before mutation, and clean rebuild after early abort.
