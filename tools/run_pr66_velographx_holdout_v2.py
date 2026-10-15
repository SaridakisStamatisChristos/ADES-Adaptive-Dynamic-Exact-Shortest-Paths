#!/usr/bin/env python3
"""Corrected PR66 preregistration after the v1 workload preflight falsified locality=0.

No v1 performance result is reused.  The execution engine/statistical pass rule
remain exactly those in run_pr66_velographx_holdout.py; this wrapper changes only
the source-policy parameters needed to realize the already-stated source-reuse
intent and uses entirely fresh seeds.
"""

import run_pr66_velographx_holdout as holdout

holdout.SEEDS = [701, 809, 907]
holdout.FAMILIES = [
    {
        "name": "uniform",
        "generator": "uniform",
        "hot_sources": 4,
        "epoch_queries": 20,
        "locality_percent": 0,
    },
    {
        "name": "hot-pool-2",
        "generator": "hot-pool",
        "hot_sources": 2,
        "epoch_queries": 20,
        "locality_percent": 100,
    },
    {
        "name": "hot-pool-4",
        "generator": "hot-pool",
        "hot_sources": 4,
        "epoch_queries": 20,
        "locality_percent": 100,
    },
    {
        "name": "rotating-hot-4",
        "generator": "rotating-hot",
        "hot_sources": 4,
        "epoch_queries": 20,
        "locality_percent": 100,
    },
]

if __name__ == "__main__":
    try:
        holdout.main()
    except Exception as exc:
        import sys
        print(f"PR66 VeloGraphX corrected holdout failed: {exc}", file=sys.stderr)
        raise
