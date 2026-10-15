#!/usr/bin/env python3
"""Empirical preflight for the frozen NetworKit DynDijkstra comparator candidate.

This deliberately does NOT promote the comparator to DIRECT. It probes only the
problem-domain correctness gates that can be established on small adversarial and
random directed weighted graphs. Adapter, memory-budget and timing gates remain
separate work even if every case here passes.

Each case is executed in an isolated subprocess. This is intentional: a native
DynDijkstra update that fails to terminate on an admissible tiny graph must be
preserved as qualification evidence instead of consuming the whole GitHub Actions
job and suppressing all other results.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import subprocess
import sys
from pathlib import Path

import networkit as nk

PINNED_COMMIT = "ab420840dbb8a61fcb1ea0d89e5acc44fe66f2ab"
CASE_NAMES = (
    "simple-directed-increase-decrease",
    "zero-weight-equal-distance",
    "asymmetric-directed-arcs",
    "random-directed-mixed",
)


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
    print(f"[{name}] initial DynDijkstra.run()", flush=True)
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
        print(
            f"[{name}] step={step} edge=({u},{v}) old={old_weight} new={new_weight} begin",
            flush=True,
        )
        updater.update([event])
        dynamic.update(event)
        print(f"[{name}] step={step} dynamic.update complete", flush=True)

        mismatches = compare(dynamic, g, source)
        observations.append({
            "step": step,
            "edge": [u, v],
            "old_weight": old_weight,
            "new_weight": float(new_weight),
            "mismatch_count": len(mismatches),
            "mismatches": mismatches[:8],
        })

    passed = not initial and all(not x["mismatch_count"] for x in observations)
    return {
        "name": name,
        "outcome": "PASS" if passed else "MISMATCH",
        "directed": bool(g.isDirected()),
        "weighted": bool(g.isWeighted()),
        "source": source,
        "initial_mismatch_count": len(initial),
        "increase_updates": increases,
        "decrease_updates": decreases,
        "steps": observations,
        "pass": passed,
    }


def simple_increase_decrease() -> dict:
    g = weighted_directed(5)
    for u, v, w in [
        (0, 1, 1.0), (1, 2, 1.0), (0, 2, 5.0), (2, 3, 1.0),
        (1, 3, 10.0), (3, 4, 1.0), (0, 4, 50.0),
    ]:
        g.addEdge(u, v, w)
    return run_sequence("simple-directed-increase-decrease", g, 0, [
        (1, 2, 10.0),
        (0, 2, 1.0),
        (2, 3, 20.0),
        (1, 3, 1.0),
    ])


def zero_weight_equal_distance() -> dict:
    g = weighted_directed(7)
    for u, v, w in [
        (0, 1, 0.0), (0, 2, 0.0), (1, 3, 0.0), (2, 3, 0.0),
        (3, 4, 1.0), (1, 5, 2.0), (2, 5, 2.0), (5, 6, 1.0),
    ]:
        g.addEdge(u, v, w)
    return run_sequence("zero-weight-equal-distance", g, 0, [
        (1, 3, 1.0),
        (2, 3, 2.0),
        (1, 3, 0.0),
        (0, 2, 3.0),
    ])


def asymmetric_direction_case() -> dict:
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
        if rng.random() < 0.5:
            new_weight = min(40.0, old + float(1 + rng.randrange(10)))
        else:
            new_weight = float(rng.randrange(0, 8))
        updates.append((u, v, new_weight))
    return run_sequence("random-directed-mixed", g, 0, updates)


def execute_case(name: str) -> dict:
    if name == "simple-directed-increase-decrease":
        return simple_increase_decrease()
    if name == "zero-weight-equal-distance":
        return zero_weight_equal_distance()
    if name == "asymmetric-directed-arcs":
        return asymmetric_direction_case()
    if name == "random-directed-mixed":
        return randomized_mixed()
    raise ValueError(f"unknown case: {name}")


def run_isolated_case(name: str, out: Path, timeout_seconds: int) -> dict:
    case_path = out / f"case-{name}.json"
    cmd = [
        sys.executable,
        str(Path(__file__).resolve()),
        "--case",
        name,
        "--case-out",
        str(case_path),
    ]
    print(f"[driver] starting {name} with timeout={timeout_seconds}s", flush=True)
    try:
        completed = subprocess.run(cmd, timeout=timeout_seconds, check=False)
    except subprocess.TimeoutExpired:
        result = {
            "name": name,
            "outcome": "TIMEOUT",
            "pass": False,
            "timeout_seconds": timeout_seconds,
            "increase_updates": 0,
            "decrease_updates": 0,
            "steps": [],
        }
        case_path.write_text(json.dumps(result, indent=2) + "\n")
        print(f"[driver] {name}: TIMEOUT after {timeout_seconds}s", flush=True)
        return result

    if completed.returncode != 0:
        result = {
            "name": name,
            "outcome": "PROCESS_ERROR",
            "pass": False,
            "returncode": completed.returncode,
            "increase_updates": 0,
            "decrease_updates": 0,
            "steps": [],
        }
        case_path.write_text(json.dumps(result, indent=2) + "\n")
        print(f"[driver] {name}: PROCESS_ERROR rc={completed.returncode}", flush=True)
        return result

    result = json.loads(case_path.read_text())
    print(f"[driver] {name}: {result['outcome']}", flush=True)
    return result


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--case", choices=CASE_NAMES)
    ap.add_argument("--case-out")
    ap.add_argument("--case-timeout", type=int, default=60)
    args = ap.parse_args()

    if args.case:
        if not args.case_out:
            ap.error("--case-out is required with --case")
        result = execute_case(args.case)
        case_out = Path(args.case_out)
        case_out.parent.mkdir(parents=True, exist_ok=True)
        case_out.write_text(json.dumps(result, indent=2) + "\n")
        return

    if not args.out:
        ap.error("--out is required in driver mode")

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    cases = [run_isolated_case(name, out, args.case_timeout) for name in CASE_NAMES]

    completed_cases = [c for c in cases if c["outcome"] in {"PASS", "MISMATCH"}]
    increase_updates = sum(c.get("increase_updates", 0) for c in completed_cases)
    decrease_updates = sum(c.get("decrease_updates", 0) for c in completed_cases)
    any_timeout = any(c["outcome"] == "TIMEOUT" for c in cases)
    any_process_error = any(c["outcome"] == "PROCESS_ERROR" for c in cases)
    any_mismatch = any(c["outcome"] == "MISMATCH" for c in cases)
    semantic_pass = all(c["outcome"] == "PASS" for c in cases)

    if semantic_pass:
        exact_gate = "PASS"
    elif any_mismatch:
        exact_gate = "FAIL"
    else:
        exact_gate = "UNRESOLVED"

    report = {
        "schema": "ades-networkit-dyndijkstra-semantic-preflight-v2",
        "networkit_pinned_commit": PINNED_COMMIT,
        "uses_edge_weight_update_events": True,
        "case_timeout_seconds": args.case_timeout,
        "directed_weighted_cases": len(cases),
        "completed_cases": len(completed_cases),
        "increase_updates": increase_updates,
        "decrease_updates": decrease_updates,
        "termination_gate": "FAIL" if any_timeout else ("UNRESOLVED" if any_process_error else "PASS"),
        "semantic_gates": {
            "exact_answers": exact_gate,
            "directed_graphs": "PASS" if semantic_pass else "UNRESOLVED",
            "nonnegative_weighted_graphs": "PASS" if semantic_pass else "UNRESOLVED",
            "weight_increases": "PASS" if semantic_pass and increase_updates else "UNRESOLVED",
            "weight_decreases": "PASS" if semantic_pass and decrease_updates else "UNRESOLVED",
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
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
