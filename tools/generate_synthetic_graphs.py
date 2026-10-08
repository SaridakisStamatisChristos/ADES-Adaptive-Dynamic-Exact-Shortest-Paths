#!/usr/bin/env python3
"""Generate the deterministic non-road graphs pinned by PR47.

The output is canonical uncompressed DIMACS shortest-path text.  zlib's gzopen()
reads these files transparently, so ADES can consume them without introducing
platform-sensitive gzip headers into the dataset hash.
"""

from __future__ import annotations

import argparse
from pathlib import Path

MASK64 = (1 << 64) - 1


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & MASK64
    z = x
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    return (z ^ (z >> 31)) & MASK64


class Rng:
    def __init__(self, seed: int) -> None:
        self.state = seed & MASK64

    def next(self) -> int:
        self.state = splitmix64(self.state)
        return self.state

    def below(self, bound: int) -> int:
        if bound <= 0:
            raise ValueError("bound must be positive")
        return self.next() % bound


def edge_weight(seed: int, u: int, v: int) -> int:
    mixed = (seed ^ ((u + 1) << 32) ^ (v + 1)) & MASK64
    return 1 + splitmix64(mixed) % 100_000


def render(graph_id: str, n: int, edges: set[tuple[int, int]], seed: int) -> bytes:
    lines = [
        "c ADES synthetic dataset registry v1\n",
        f"c id {graph_id}\n",
        f"p sp {n} {len(edges)}\n",
    ]
    for u, v in sorted(edges):
        lines.append(f"a {u + 1} {v + 1} {edge_weight(seed, u, v)}\n")
    return "".join(lines).encode("ascii")


def grid() -> tuple[str, int, set[tuple[int, int]], int]:
    graph_id = "syn-grid-224x224-v1"
    rows = cols = 224
    n = rows * cols
    seed = 0xA47E5
    edges: set[tuple[int, int]] = set()
    for r in range(rows):
        for c in range(cols):
            u = r * cols + c
            if c + 1 < cols:
                v = u + 1
                edges.add((u, v))
                edges.add((v, u))
            if r + 1 < rows:
                v = u + cols
                edges.add((u, v))
                edges.add((v, u))
    return graph_id, n, edges, seed


def uniform_random() -> tuple[str, int, set[tuple[int, int]], int]:
    graph_id = "syn-uniform-50000-d6-v1"
    n = 50_000
    degree = 6
    seed = 0x147A11
    rng = Rng(seed)
    edges: set[tuple[int, int]] = set()
    for u in range(n):
        chosen: set[int] = set()
        while len(chosen) < degree:
            v = rng.below(n - 1)
            if v >= u:
                v += 1
            chosen.add(v)
        for v in chosen:
            edges.add((u, v))
    return graph_id, n, edges, seed


def scale_free() -> tuple[str, int, set[tuple[int, int]], int]:
    graph_id = "syn-scale-free-50000-m4-v1"
    n = 50_000
    m = 4
    seed = 0x5CA1EF
    rng = Rng(seed)
    edges: set[tuple[int, int]] = set()
    degree = [0] * n
    initial = m + 1

    for u in range(initial):
        for v in range(u + 1, initial):
            edges.add((u, v))
            edges.add((v, u))
            degree[u] += 1
            degree[v] += 1

    pool: list[int] = []
    for u in range(initial):
        pool.extend([u] * degree[u])

    for u in range(initial, n):
        selected: set[int] = set()
        while len(selected) < m:
            selected.add(pool[rng.below(len(pool))])
        for v in sorted(selected):
            edges.add((u, v))
            edges.add((v, u))
            degree[u] += 1
            degree[v] += 1
            pool.append(v)
        pool.extend([u] * degree[u])

    return graph_id, n, edges, seed


def small_world() -> tuple[str, int, set[tuple[int, int]], int]:
    graph_id = "syn-small-world-50000-k8-r10-v1"
    n = 50_000
    half_degree = 4
    seed = 0x5A411
    rng = Rng(seed)
    edges: set[tuple[int, int]] = set()

    for u in range(n):
        for step in range(1, half_degree + 1):
            for ring_target in ((u + step) % n, (u - step) % n):
                v = ring_target
                if rng.below(100) < 10:
                    while True:
                        candidate = rng.below(n - 1)
                        if candidate >= u:
                            candidate += 1
                        if (u, candidate) not in edges:
                            v = candidate
                            break
                edges.add((u, v))

    return graph_id, n, edges, seed


def clustered() -> tuple[str, int, set[tuple[int, int]], int]:
    graph_id = "syn-clustered-50000-c100-d6-v1"
    n = 50_000
    communities = 100
    community_size = n // communities
    local_degree = 5
    seed = 0xC1A57
    rng = Rng(seed)
    edges: set[tuple[int, int]] = set()

    for u in range(n):
        community = u // community_size
        start = community * community_size
        self_offset = u - start
        chosen: set[int] = set()
        while len(chosen) < local_degree:
            offset = rng.below(community_size - 1)
            if offset >= self_offset:
                offset += 1
            chosen.add(start + offset)
        for v in chosen:
            edges.add((u, v))

        while True:
            v = rng.below(n)
            if v // community_size != community:
                break
        edges.add((u, v))

    return graph_id, n, edges, seed


GENERATORS = {
    "syn-grid-224x224-v1": grid,
    "syn-uniform-50000-d6-v1": uniform_random,
    "syn-scale-free-50000-m4-v1": scale_free,
    "syn-small-world-50000-k8-r10-v1": small_world,
    "syn-clustered-50000-c100-d6-v1": clustered,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--only", choices=sorted(GENERATORS))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    selected = [args.only] if args.only else sorted(GENERATORS)
    for graph_id in selected:
        generated_id, n, edges, seed = GENERATORS[graph_id]()
        assert generated_id == graph_id
        path = args.output_dir / f"{graph_id}.gr"
        path.write_bytes(render(graph_id, n, edges, seed))
        print(f"{graph_id} vertices={n} arcs={len(edges)} path={path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
