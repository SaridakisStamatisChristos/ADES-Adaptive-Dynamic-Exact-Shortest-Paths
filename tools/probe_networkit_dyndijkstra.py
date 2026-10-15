#!/usr/bin/env python3
"""Empirical preflight for the frozen NetworKit DynDijkstra comparator candidate.

This deliberately does NOT promote the comparator to DIRECT.  It probes only the
problem-domain correctness gates that can be established on small adversarial and
random directed weighted graphs.  Adapter, memory-budget and timing gates remain
separate work even if every case here passes.
"""
from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path

import networkit as nk

PINNED_COMMIT = "ab420840dbb8a61fcb1ea0d89e5acc44fe66f2ab"


def same_distance(a: float, b: float) -> bool:
    a_inf = (not math.isfinite(a)) or a > 1e100
    b_inf = (not math.isfinite(b)) or b > 1e100
    if a_inf or b_inf:
        return a_inf and b_inf
    return math.isclose(a, b, rel_tol=1e-10, abs_tol=1e-10)


def fresh(g: nk.Graph, source: int) -> list[float]:
    return list(nk.distance.Dijkstra(g, source).run().getDistances())


def compare(dynamic: nk.distance.DynDijkstra, g: nk.Graph, source: int) -> list[dict]:
    got = list(dynamic.getDistances())
    want = fresh(g, source)
    mismatches = []
    for node, (actual, expected) in enumerate(zip(got, want)):
        if not same_distance(float(actual), float(expected)):
            mismatches.append({"node": node, "dynamic": float(actual), "fresh": float(expected)})
    return mismatches


def weighted_directed(n: int) -> nk.Graph:
    return nk.Graph(n, weighted=True, directed=True)


def run_sequence(name: str, g: nk.Graph, source: int, updates: list[tuple[int, int, float]]) -> dict:
    dynamic = nk.distance.DynDijkstra(g, source)
    dynamic.run()
    updater = nk.dynamics.GraphUpdater(g)
    initial = compare(dynamic, g, source)
    observations = []
    increases = 0
    decreases = 0
    for step, (u, v, new_weight) in enumerate(updates):
        old_weight = float(g.weight(u, v))
        if new_weight > old_weight:
            increases += 1
        elif new_weight < old_weight:
            decreases += 1
        event = nk.dynamics.GraphEvent(
            nk.dynamics.GraphEventType.EDGE_WEIGHT_UPDATE, u, v, float(new_weight)
        )
        updater.update([event])
        dynamic.update(event)
        mismatches = compare(dynamic, g, source)
        observations.append({
            "step": step,
            "edge": [u, v],
            "old_weight": old_weight,
            "new_weight": float(new_weight),
            "mismatch_count": len(mismatches),
            "mismatches": mismatches[:8],
        })
    return {
        "name": name,
        "directed": bool(g.isDirected()),
        "weighted": bool(g.isWeighted()),
        "source": source,
        "initial_mismatch_count": len(initial),
        "increase_updates": increases,
        "decrease_updates": decreases,
        "steps": observations,
        "pass": not initial and all(not x["mismatch_count"] for x in observations),
    }


def simple_increase_decrease() -> dict:
    g = weighted_directed(5)
    for u, v, w in [
        (0, 1, 1.0), (1, 2, 1.0), (0, 2, 5.0), (2, 3, 1.0),
        (1, 3, 10.0), (3, 4, 1.0), (0, 4, 50.0),
    ]:
        g.addEdge(u, v, w)
    return run_sequence("simple-directed-increase-decrease", g, 0, [
        (1, 2, 10.0),  # shortest-tree edge increase
        (0, 2, 1.0),   # non-tree edge decrease becomes shortest
        (2, 3, 20.0),  # another increase
        (1, 3, 1.0),   # decrease creates replacement route
    ])


def zero_weight_equal_distance() -> dict:
    g = weighted_directed(7)
    for u, v, w in [
        (0, 1, 0.0), (0, 2, 0.0), (1, 3, 0.0), (2, 3, 0.0),
        (3, 4, 1.0), (1, 5, 2.0), (2, 5, 2.0), (5, 6, 1.0),
    ]:
        g.addEdge(u, v, w)
    return run_sequence("zero-weight-equal-distance", g, 0, [
        (1, 3, 1.0),   # alternate equal-distance predecessor must preserve d(3)
        (2, 3, 2.0),   # now d(3) must increase
        (1, 3, 0.0),   # directed decrease restores a zero path
        (0, 2, 3.0),   # source-adjacent increase changes one equal branch
    ])


def asymmetric_direction_case() -> dict:
    # Reverse arcs deliberately have very different weights.  Any accidental
    # undirected interpretation is exposed immediately by fresh-Dijkstra parity.
    g = weighted_directed(6)
    for u, v, w in [
        (0, 1, 1.0), (1, 0, 30.0),
        (1, 2, 1.0), (2, 1, 40.0),
        (0, 3, 8.0), (3, 2, 1.0),
        (2, 4, 1.0), (4, 2, 50.0),
        (4, 5, 1.0), (1, 5, 20.0),
    ]:
        g.addEdge(u, v, w)
    return run_sequence("asymmetric-directed-arcs", g, 0, [
        (1, 2, 15.0),
        (0, 3, 2.0),
        (3, 2, 20.0),
        (1, 5, 2.0),
    ])


def randomized_mixed(seed: int = 20261015) -> dict:
    rng = random.Random(seed)
    n = 32
    g = weighted_directed(n)
    edges: set[tuple[int, int]] = set()

    # Directed ring guarantees source reachability; random asymmetric arcs add
    # cycles, ties and alternate routes without relying on generator semantics.
    for u in range(n):
        v = (u + 1) % n
        g.addEdge(u, v, float(1 + rng.randrange(5)))
        edges.add((u, v))
    while len(edges) < 180:
        u, v = rng.randrange(n), rng.randrange(n)
        if u == v or (u, v) in edges:
            continue
        g.addEdge(u, v, float(rng.randrange(0, 16)))
        edges.add((u, v))

    edge_list = sorted(edges)
    updates = []
    for _ in range(120):
        u, v = rng.choice(edge_list)
        old = float(g.weight(u, v))
        # The sequence fed to run_sequence is generated from the initial graph,
        # so alternate increases/decreases around a broad weight range rather
        # than trying to encode deltas.
        if rng.random() < 0.5:
            new_weight = min(40.0, old + float(1 + rng.randrange(10)))
        else:
            new_weight = float(rng.randrange(0, 8))
        updates.append((u, v, new_weight))
    return run_sequence("random-directed-mixed", g, 0, updates)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    cases = [
        simple_increase_decrease(),
        zero_weight_equal_distance(),
        asymmetric_direction_case(),
        randomized_mixed(),
    ]
    increase_updates = sum(c["increase_updates"] for c in cases)
    decrease_updates = sum(c["decrease_updates"] for c in cases)
    semantic_pass = all(c["pass"] for c in cases)
    report = {
        "schema": "ades-networkit-dyndijkstra-semantic-preflight-v1",
        "networkit_pinned_commit": PINNED_COMMIT,
        "uses_edge_weight_update_events": True,
        "directed_weighted_cases": len(cases),
        "increase_updates": increase_updates,
        "decrease_updates": decrease_updates,
        "semantic_gates": {
            "exact_answers": "PASS" if semantic_pass else "FAIL",
            "directed_graphs": "PASS" if semantic_pass else "FAIL",
            "nonnegative_weighted_graphs": "PASS" if semantic_pass else "FAIL",
            "weight_increases": "PASS" if semantic_pass and increase_updates else "FAIL",
            "weight_decreases": "PASS" if semantic_pass and decrease_updates else "FAIL",
        },
        "semantic_preflight_pass": semantic_pass,
        "direct_status_after_this_probe": "PENDING",
        "remaining_direct_gates": [
            "arbitrary_query_sources_supported",
            "matched_trace_adapter",
            "independent_oracle_validation_on_ADES_traces",
            "persistent_memory_budget_enforceable",
            "persistent_memory_accounting_auditable",
            "timing_boundary_declared",
        ],
        "cases": cases,
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
