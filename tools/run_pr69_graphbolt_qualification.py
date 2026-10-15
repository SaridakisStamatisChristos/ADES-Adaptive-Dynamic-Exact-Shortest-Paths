#!/usr/bin/env python3
"""PR69 semantic qualification for pinned GraphBolt/DZiG weighted SSSP.

No performance claim is made here.  The test exercises the upstream weighted
SSSP executable through its documented WeightedAdjacencyGraph + streaming
edge-operation interface.  A logical weight replacement is represented as two
separate upstream operations: delete (u,v,old), then add (u,v,new).  GraphBolt's
full all-vertex output is checked against a fresh Python Dijkstra oracle after
initialization and after every deletion/addition phase.

The graph is deliberately small and all finite path lengths are kept far below
GraphBolt SSSP's uint16_t/MAX_DISTANCE=65535 application boundary.
"""
from __future__ import annotations

import argparse
import heapq
import json
import os
from pathlib import Path
import random
import subprocess

GRAPHBOLT_COMMIT = "7c2792cc77d59b9722288ded8c3c62f6a9a87fc7"
MAX_DISTANCE = 65535
N = 64
INITIAL_EDGES = 320
REPLACEMENTS = 40
SEEDS = [1201, 2203, 3209]
SOURCES = [0, 11, 37]


def make_graph(seed: int) -> dict[tuple[int, int], int]:
    rng = random.Random(seed)
    edges: dict[tuple[int, int], int] = {}
    # Strongly connected directed cycle backbone.
    for u in range(N):
        edges[(u, (u + 1) % N)] = rng.randint(1, 20)
    while len(edges) < INITIAL_EDGES:
        u = rng.randrange(N)
        v = rng.randrange(N)
        if u == v or (u, v) in edges:
            continue
        edges[(u, v)] = rng.randint(1, 40)
    return edges


def write_weighted_adjacency(path: Path, edges: dict[tuple[int, int], int]) -> None:
    by_src: list[list[tuple[int, int]]] = [[] for _ in range(N)]
    for (u, v), w in sorted(edges.items()):
        by_src[u].append((v, w))
    offsets = []
    destinations = []
    weights = []
    offset = 0
    for u in range(N):
        offsets.append(offset)
        for v, w in by_src[u]:
            destinations.append(v)
            weights.append(w)
            offset += 1
    if len(destinations) != len(edges):
        raise AssertionError("edge-count mismatch")
    tokens = ["WeightedAdjacencyGraph", str(N), str(len(edges))]
    tokens.extend(str(x) for x in offsets)
    tokens.extend(str(x) for x in destinations)
    tokens.extend(str(x) for x in weights)
    path.write_text("\n".join(tokens) + "\n", encoding="ascii")


def dijkstra(edges: dict[tuple[int, int], int], source: int) -> list[int]:
    adj: list[list[tuple[int, int]]] = [[] for _ in range(N)]
    for (u, v), w in edges.items():
        adj[u].append((v, w))
    dist = [MAX_DISTANCE] * N
    dist[source] = 0
    pq = [(0, source)]
    while pq:
        du, u = heapq.heappop(pq)
        if du != dist[u]:
            continue
        for v, w in adj[u]:
            nd = du + w
            if nd >= MAX_DISTANCE:
                raise AssertionError("qualification graph exceeded GraphBolt uint16 distance boundary")
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(pq, (nd, v))
    return dist


def make_replacements(seed: int, base: dict[tuple[int, int], int]) -> list[tuple[int, int, int, int]]:
    rng = random.Random(seed ^ 0x69B017)
    current = dict(base)
    keys = list(sorted(current))
    chosen = rng.sample(keys, REPLACEMENTS)
    out = []
    for i, key in enumerate(chosen):
        old = current[key]
        if i % 2 == 0:
            # decrease, but retain positive weight
            new = max(1, old - rng.randint(1, min(old, 7)))
            if new == old:
                new = old + 1
        else:
            new = old + rng.randint(1, 9)
        current[key] = new
        out.append((key[0], key[1], old, new))
    return out


def write_stream(path: Path, replacements: list[tuple[int, int, int, int]]) -> None:
    lines = []
    for u, v, old, new in replacements:
        lines.append(f"d {u} {v} {old}\n")
        lines.append(f"a {u} {v} {new}\n")
    path.write_text("".join(lines), encoding="ascii")


def parse_graphbolt_output(path: Path) -> list[int]:
    if not path.exists():
        raise RuntimeError(f"missing GraphBolt output: {path}")
    dist = [None] * N
    for raw in path.read_text(encoding="utf-8").splitlines():
        parts = raw.split()
        if len(parts) < 4:
            raise RuntimeError(f"malformed GraphBolt output line: {raw!r}")
        v = int(parts[0])
        value = int(parts[-1])
        if not 0 <= v < N:
            raise RuntimeError(f"vertex out of range in GraphBolt output: {v}")
        dist[v] = value
    if any(x is None for x in dist):
        raise RuntimeError("GraphBolt output did not contain every vertex")
    return [int(x) for x in dist]


def first_mismatch(got: list[int], expected: list[int]):
    for v, (a, b) in enumerate(zip(got, expected)):
        if a != b:
            return {"vertex": v, "graphbolt": a, "fresh_dijkstra": b}
    return None


def run_case(exe: str, work: Path, seed: int, source: int) -> dict:
    case = work / f"seed-{seed}__source-{source}"
    case.mkdir(parents=True, exist_ok=True)
    base = make_graph(seed)
    replacements = make_replacements(seed ^ (source << 8), base)
    graph_path = case / "initial.adj"
    stream_path = case / "stream.txt"
    out_prefix = case / "out-"
    write_weighted_adjacency(graph_path, base)
    write_stream(stream_path, replacements)

    env = os.environ.copy()
    env.setdefault("CILK_NWORKERS", "1")
    cmd = [
        exe,
        "-source", str(source),
        "-numberOfUpdateBatches", str(2 * len(replacements)),
        "-nEdges", "1",
        "-fixedBatchSize",
        "-simple",
        "-enforceEdgeValidity",
        "-streamPath", str(stream_path),
        "-outputFile", str(out_prefix),
        str(graph_path),
    ]
    proc = subprocess.run(cmd, text=True, capture_output=True, env=env)
    (case / "stdout.txt").write_text(proc.stdout, encoding="utf-8")
    (case / "stderr.txt").write_text(proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        return {
            "seed": seed, "source": source, "exact": False, "execution_failed": True,
            "returncode": proc.returncode,
            "stderr_tail": proc.stderr[-4000:],
        }

    state = dict(base)
    checks = 0
    expected = dijkstra(state, source)
    got = parse_graphbolt_output(Path(f"{out_prefix}0"))
    checks += 1
    mismatch = first_mismatch(got, expected)
    if mismatch:
        return {"seed": seed, "source": source, "exact": False, "checks": checks,
                "first_mismatch": {"phase": "initial", **mismatch}}

    batch = 0
    for ridx, (u, v, old, new) in enumerate(replacements):
        if state.get((u, v)) != old:
            raise AssertionError("replacement state mismatch")
        del state[(u, v)]
        batch += 1
        expected = dijkstra(state, source)
        got = parse_graphbolt_output(Path(f"{out_prefix}{batch}"))
        checks += 1
        mismatch = first_mismatch(got, expected)
        if mismatch:
            return {"seed": seed, "source": source, "exact": False, "checks": checks,
                    "first_mismatch": {"phase": "after_delete", "replacement": ridx,
                                       "edge": [u, v], "old": old, "new": new, **mismatch}}

        state[(u, v)] = new
        batch += 1
        expected = dijkstra(state, source)
        got = parse_graphbolt_output(Path(f"{out_prefix}{batch}"))
        checks += 1
        mismatch = first_mismatch(got, expected)
        if mismatch:
            return {"seed": seed, "source": source, "exact": False, "checks": checks,
                    "first_mismatch": {"phase": "after_add", "replacement": ridx,
                                       "edge": [u, v], "old": old, "new": new, **mismatch}}

    return {
        "seed": seed,
        "source": source,
        "exact": True,
        "execution_failed": False,
        "checks": checks,
        "replacements": len(replacements),
        "stream_batches": 2 * len(replacements),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--graphbolt", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    cases = [run_case(args.graphbolt, out / "cases", seed, source)
             for seed in SEEDS for source in SOURCES]
    all_exact = all(c.get("exact") for c in cases)
    result = {
        "schema": "ades-pr69-graphbolt-weighted-sssp-qualification-v1",
        "graphbolt_commit": GRAPHBOLT_COMMIT,
        "upstream_algorithm_modified": False,
        "semantic_scope": "directed fixed-source positive-integer weighted SSSP under edge add/delete; logical weight replacement encoded as delete(old)+add(new)",
        "application_distance_boundary": MAX_DISTANCE,
        "vertices": N,
        "initial_edges": INITIAL_EDGES,
        "seeds": SEEDS,
        "sources": SOURCES,
        "replacements_per_case": REPLACEMENTS,
        "case_count": len(cases),
        "exact_case_count": sum(1 for c in cases if c.get("exact")),
        "checks": sum(c.get("checks", 0) for c in cases),
        "all_exact": all_exact,
        "qualification": (
            "QUALIFIED_DIRECTED_FIXED_SOURCE_WEIGHTED_STREAMING_SSSP_WITH_UINT16_DISTANCE_BOUND"
            if all_exact else "NOT_QUALIFIED"
        ),
        "first_failure": next((c for c in cases if not c.get("exact")), None),
        "cases": cases,
    }
    (out / "qualification.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                             encoding="utf-8")
    print(json.dumps({k: result[k] for k in
                      ["qualification", "case_count", "exact_case_count", "checks", "first_failure"]},
                     indent=2, sort_keys=True))
    return 0 if all_exact else 1


if __name__ == "__main__":
    raise SystemExit(main())
