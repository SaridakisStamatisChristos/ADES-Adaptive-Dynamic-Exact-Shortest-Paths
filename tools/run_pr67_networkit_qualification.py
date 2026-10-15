#!/usr/bin/env python3
"""PR67: semantic qualification for NetworKit DynDijkstra.

This is deliberately not a performance benchmark. It asks whether a pinned
NetworKit release can serve as an exact dynamic-maintenance comparator under
well-defined graph/update semantics before any timing result is observed.

Weight replacement is represented using only operations DynDijkstra explicitly
supports: remove the old edge, update the dynamic state, add the same edge with
its new weight, update the dynamic state.  Exactness is checked against fresh
NetworKit Dijkstra after both phases.
"""

from __future__ import annotations

import argparse
import json
import math
import random
from dataclasses import dataclass
from pathlib import Path

import networkit as nk

PINNED_VERSION = "11.2.2"


@dataclass(frozen=True)
class Edge:
    u: int
    v: int
    w: int


def close_distance(a: float, b: float) -> bool:
    # All finite test weights/distances are integers exactly representable in
    # double. NetworKit uses max-double as its unreachable sentinel.
    if a > 1e300 and b > 1e300:
        return True
    return a == b


def fresh_distances(g: nk.Graph, source: int) -> list[float]:
    alg = nk.distance.Dijkstra(g, source, storePaths=False)
    alg.run()
    return list(alg.getDistances())


def compare_all(g: nk.Graph, dyn: nk.distance.DynDijkstra, source: int):
    got = list(dyn.getDistances())
    expected = fresh_distances(g, source)
    for node, (a, b) in enumerate(zip(got, expected)):
        if not close_distance(a, b):
            return {
                "source": source,
                "node": node,
                "dynamic": a,
                "fresh": b,
            }
    return None


def build_graph(n: int, edges: list[Edge], directed: bool) -> nk.Graph:
    g = nk.Graph(n, weighted=True, directed=directed)
    for e in edges:
        g.addEdge(e.u, e.v, float(e.w))
    return g


def make_edges(seed: int, *, directed: bool, n: int = 48, extra: int = 144) -> list[Edge]:
    rng = random.Random(seed)
    edges: dict[tuple[int, int], int] = {}

    # A connected/spanning backbone. For the directed case this is a directed
    # cycle, so every node remains reachable from every source before updates.
    for u in range(n):
        v = (u + 1) % n
        key = (u, v) if directed else (min(u, v), max(u, v))
        edges[key] = rng.randint(2, 40)

    while len(edges) < n + extra:
        u = rng.randrange(n)
        v = rng.randrange(n)
        if u == v:
            continue
        key = (u, v) if directed else (min(u, v), max(u, v))
        if key in edges:
            continue
        edges[key] = rng.randint(1, 50)

    return [Edge(u, v, w) for (u, v), w in sorted(edges.items())]


def run_case(seed: int, source: int, *, directed: bool, updates: int = 80) -> dict:
    initial = make_edges(seed, directed=directed)
    g = build_graph(48, initial, directed)
    dyn = nk.distance.DynDijkstra(g, source)
    dyn.run()

    first = compare_all(g, dyn, source)
    if first is not None:
        return {
            "seed": seed,
            "source": source,
            "directed": directed,
            "checks": 1,
            "exact": False,
            "first_mismatch": {"phase": "initial", **first},
        }

    current = {(e.u, e.v): e.w for e in initial}
    keys = list(current)
    rng = random.Random(seed ^ (source << 16) ^ (0xD1CE if directed else 0xA11CE))
    checks = 1
    first_mismatch = None

    for step in range(updates):
        u, v = keys[rng.randrange(len(keys))]
        old = current[(u, v)]
        if step % 2 == 0:
            new = max(1, old - rng.randint(1, min(old, 9)))
            if new == old:
                new = old + 1
        else:
            new = old + rng.randint(1, 11)

        # Phase 1: exact deletion state.
        g.removeEdge(u, v)
        dyn.update(nk.dynamics.GraphEvent(nk.dynamics.GraphEventType.EDGE_REMOVAL, u, v, float(old)))
        checks += 1
        mismatch = compare_all(g, dyn, source)
        if mismatch is not None:
            first_mismatch = {
                "phase": "after_removal",
                "step": step,
                "edge": [u, v],
                "old_weight": old,
                "new_weight": new,
                **mismatch,
            }
            break

        # Phase 2: same topology with replacement weight.
        g.addEdge(u, v, float(new))
        dyn.update(nk.dynamics.GraphEvent(nk.dynamics.GraphEventType.EDGE_ADDITION, u, v, float(new)))
        current[(u, v)] = new
        checks += 1
        mismatch = compare_all(g, dyn, source)
        if mismatch is not None:
            first_mismatch = {
                "phase": "after_readdition",
                "step": step,
                "edge": [u, v],
                "old_weight": old,
                "new_weight": new,
                **mismatch,
            }
            break

    return {
        "seed": seed,
        "source": source,
        "directed": directed,
        "checks": checks,
        "exact": first_mismatch is None,
        "first_mismatch": first_mismatch,
    }


def summarize(cases: list[dict]) -> dict:
    return {
        "case_count": len(cases),
        "exact_case_count": sum(1 for c in cases if c["exact"]),
        "checks": sum(c["checks"] for c in cases),
        "all_exact": all(c["exact"] for c in cases),
        "first_failure": next((c for c in cases if not c["exact"]), None),
        "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    version = getattr(nk, "__version__", "unknown")
    if version != PINNED_VERSION:
        raise SystemExit(f"expected NetworKit {PINNED_VERSION}, got {version}")

    seeds = [1103, 1601, 2203]
    sources = [0, 7, 23]

    undirected_cases = [
        run_case(seed, source, directed=False) for seed in seeds for source in sources
    ]
    directed_cases = [
        run_case(seed, source, directed=True) for seed in seeds for source in sources
    ]

    result = {
        "schema": "ades-pr67-networkit-semantic-qualification-v1",
        "networkit_version": version,
        "comparator": "NetworKit DynDijkstra",
        "weight_update_mapping": "EDGE_REMOVAL(old) then EDGE_ADDITION(new)",
        "finite_weights": "positive integers exactly representable as double",
        "test_design": {
            "seeds": seeds,
            "sources": sources,
            "vertices_per_case": 48,
            "initial_edges_per_case": 192,
            "weight_replacements_per_case": 80,
            "checks_after_both_remove_and_readd": True,
            "fresh_oracle": "NetworKit Dijkstra rerun from scratch",
        },
        "undirected": summarize(undirected_cases),
        "directed": summarize(directed_cases),
    }

    # Qualification rule: only undirected support is required. Directed results
    # are an adversarial scope probe and are never silently promoted.
    result["qualification"] = (
        "QUALIFIED_UNDIRECTED_FIXED_SOURCE_DYNAMIC_SSSP"
        if result["undirected"]["all_exact"]
        else "NOT_QUALIFIED"
    )
    if result["directed"]["all_exact"]:
        result["directed_scope_note"] = (
            "sampled directed probe passed; this alone does not certify general directed support"
        )
    else:
        result["directed_scope_note"] = (
            "directed probe produced a counterexample; restrict comparator scope to undirected graphs"
        )

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "qualification": result["qualification"],
        "undirected_all_exact": result["undirected"]["all_exact"],
        "undirected_checks": result["undirected"]["checks"],
        "directed_all_exact": result["directed"]["all_exact"],
        "directed_checks": result["directed"]["checks"],
        "directed_first_failure": result["directed"]["first_failure"],
    }, indent=2))
    return 0 if result["undirected"]["all_exact"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
