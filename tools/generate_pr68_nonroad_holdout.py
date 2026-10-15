#!/usr/bin/env python3
"""Generate the frozen PR68 non-road undirected holdout.

The logical topology is a simple connected Barabasi-Albert-style preferential-
attachment graph.  ADES receives an exact symmetric directed representation:
for logical edge k=(u,v,w), DIMACS edge ids 2*k and 2*k+1 are u->v and v->u.
Every logical weight replacement is therefore encoded as two adjacent canonical
UPDATE records with identical old/new weights.  NetworKit consumes the same
logical update as one undirected edge replacement; ADES is charged both arc
updates before the next query.

No timing is performed here.  The graph/workload identities are generated
solely from the preregistered constants below.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import random
from pathlib import Path

SCHEMA = "ades-pr68-nonroad-holdout-generator-v1"
GRAPH_SEEDS = [3109, 4001, 5011]
WORKLOAD_SEEDS = [6101, 7103]
N = 15_000
ATTACHMENTS = 5
QUERY_COUNT = 60
UPDATE_INTERVAL = 5
ROTATING_EPOCH_QUERIES = 30
WEIGHT_MIN = 100
WEIGHT_MAX = 1000
DELTA_MIN = 5
DELTA_MAX = 50

FAMILIES = [
    {"name": "uniform", "kind": "uniform", "hot_sources": 0},
    {"name": "hot-pool-2", "kind": "hot-pool", "hot_sources": 2},
    {"name": "hot-pool-4", "kind": "hot-pool", "hot_sources": 4},
    {"name": "rotating-hot-4", "kind": "rotating-hot", "hot_sources": 4},
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def canonical_json_sha(obj: object) -> str:
    data = json.dumps(obj, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return sha256_bytes(data)


def preferential_attachment(seed: int):
    """Return deterministic simple undirected weighted edges (u,v,w), u<v."""
    rng = random.Random(seed)
    m = ATTACHMENTS
    if N <= m + 1:
        raise ValueError("N too small")

    edges: list[tuple[int, int, int]] = []
    repeated: list[int] = []
    initial = m + 1

    # Clique seed.  repeated contains one entry per incident edge endpoint, so
    # random.choice(repeated) samples proportional to degree exactly.
    for u in range(initial):
        for v in range(u + 1, initial):
            w = rng.randint(WEIGHT_MIN, WEIGHT_MAX)
            edges.append((u, v, w))
            repeated.extend((u, v))

    for new_node in range(initial, N):
        chosen: set[int] = set()
        while len(chosen) < m:
            chosen.add(rng.choice(repeated))
        for old_node in sorted(chosen):
            u, v = old_node, new_node
            w = rng.randint(WEIGHT_MIN, WEIGHT_MAX)
            edges.append((u, v, w))
            repeated.extend((u, v))

    expected = initial * (initial - 1) // 2 + (N - initial) * m
    if len(edges) != expected:
        raise AssertionError((len(edges), expected))
    return edges


def write_symmetric_dimacs(path: Path, edges: list[tuple[int, int, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = io.BytesIO()
    raw.write(f"c PR68 deterministic symmetric representation\n".encode("ascii"))
    raw.write(f"p sp {N} {2 * len(edges)}\n".encode("ascii"))
    for u, v, w in edges:
        raw.write(f"a {u + 1} {v + 1} {w}\n".encode("ascii"))
        raw.write(f"a {v + 1} {u + 1} {w}\n".encode("ascii"))
    payload = raw.getvalue()
    with path.open("wb") as f:
        with gzip.GzipFile(filename="", mode="wb", fileobj=f, mtime=0, compresslevel=9) as gz:
            gz.write(payload)


def make_sources(rng: random.Random, family: dict) -> list[int]:
    if family["kind"] == "uniform":
        # Exact no-reuse control, not merely probabilistic near-uniformity.
        return rng.sample(range(N), QUERY_COUNT)
    if family["kind"] == "hot-pool":
        pool = rng.sample(range(N), family["hot_sources"])
        return [rng.choice(pool) for _ in range(QUERY_COUNT)]
    if family["kind"] == "rotating-hot":
        sources: list[int] = []
        used: set[int] = set()
        epochs = (QUERY_COUNT + ROTATING_EPOCH_QUERIES - 1) // ROTATING_EPOCH_QUERIES
        for epoch in range(epochs):
            available = [x for x in range(N) if x not in used]
            pool = rng.sample(available, family["hot_sources"])
            used.update(pool)
            take = min(ROTATING_EPOCH_QUERIES, QUERY_COUNT - len(sources))
            sources.extend(rng.choice(pool) for _ in range(take))
        return sources
    raise ValueError(f"unknown family {family}")


def write_trace(path: Path, edges: list[tuple[int, int, int]], *, graph_seed: int,
                workload_seed: int, family: dict) -> dict:
    rng_seed = (graph_seed << 32) ^ workload_seed ^ int(hashlib.sha256(family["name"].encode()).hexdigest()[:16], 16)
    rng = random.Random(rng_seed)
    sources = make_sources(rng, family)
    targets = []
    for source in sources:
        target = rng.randrange(N - 1)
        if target >= source:
            target += 1
        targets.append(target)

    logical_update_count = QUERY_COUNT // UPDATE_INTERVAL
    logical_ids = rng.sample(range(len(edges)), logical_update_count)
    current = [w for _, _, w in edges]

    lines: list[str] = []
    increases = 0
    decreases = 0
    logical_index = 0
    for qidx, (source, target) in enumerate(zip(sources, targets), start=1):
        lines.append(f"QUERY {source} {target}\n")
        if qidx % UPDATE_INTERVAL == 0:
            logical_id = logical_ids[logical_index]
            old = current[logical_id]
            delta = rng.randint(DELTA_MIN, DELTA_MAX)
            if logical_index % 2 == 0:
                new = old - delta
                if new <= 0:
                    raise AssertionError("preregistered positive-weight floor violated")
                decreases += 1
            else:
                new = old + delta
                increases += 1
            current[logical_id] = new
            arc0 = 2 * logical_id
            arc1 = arc0 + 1
            lines.append(f"UPDATE {arc0} {old} {new}\n")
            lines.append(f"UPDATE {arc1} {old} {new}\n")
            logical_index += 1

    if logical_index != logical_update_count:
        raise AssertionError("logical update count mismatch")
    if increases == 0 or decreases == 0:
        raise AssertionError("trace must contain both update directions")

    trace_bytes = "".join(lines).encode("ascii")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(trace_bytes)
    unique_sources = len(set(sources))
    reuse = (QUERY_COUNT - unique_sources) / QUERY_COUNT
    if family["kind"] != "uniform" and reuse < 0.80:
        raise AssertionError(f"source-reuse preregistration failed for {family['name']}: {reuse}")
    if family["kind"] == "uniform" and reuse != 0.0:
        raise AssertionError("uniform control must have zero source reuse")

    cfg = {
        "graph_seed": graph_seed,
        "workload_seed": workload_seed,
        "family": family,
        "query_count": QUERY_COUNT,
        "logical_update_count": logical_update_count,
        "raw_symmetric_arc_update_count": 2 * logical_update_count,
        "update_interval_queries": UPDATE_INTERVAL,
        "update_mode": "strict-alternating-logical-weight-replacement",
        "rotating_epoch_queries": ROTATING_EPOCH_QUERIES,
        "weight_range": [WEIGHT_MIN, WEIGHT_MAX],
        "delta_range": [DELTA_MIN, DELTA_MAX],
    }
    return {
        **cfg,
        "config_sha256": canonical_json_sha(cfg),
        "trace_sha256": sha256_bytes(trace_bytes),
        "unique_query_sources": unique_sources,
        "source_reuse_queries": QUERY_COUNT - unique_sources,
        "source_reuse_fraction": reuse,
        "logical_increase_count": increases,
        "logical_decrease_count": decreases,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    out = Path(args.out)
    graphs_dir = out / "graphs"
    cells_dir = out / "cells"
    out.mkdir(parents=True, exist_ok=True)

    design = {
        "schema": SCHEMA,
        "status": "PREREGISTERED_BY_COMMIT_BEFORE_TIMING",
        "topology_family": "barabasi-albert-style-preferential-attachment",
        "logical_graph_semantics": "simple connected undirected positive-integer weighted",
        "ades_representation": "two symmetric directed DIMACS arcs per logical undirected edge",
        "graph_seeds": GRAPH_SEEDS,
        "workload_seeds": WORKLOAD_SEEDS,
        "vertices": N,
        "attachments_per_new_vertex": ATTACHMENTS,
        "query_count": QUERY_COUNT,
        "update_interval_queries": UPDATE_INTERVAL,
        "source_families": FAMILIES,
        "rotating_epoch_queries": ROTATING_EPOCH_QUERIES,
        "weight_range": [WEIGHT_MIN, WEIGHT_MAX],
        "delta_range": [DELTA_MIN, DELTA_MAX],
        "raw_update_cost_rule": "ADES executes both symmetric arc updates; NetworKit executes one logical undirected edge replacement as remove+add",
        "query_exactness_boundary": "oracle/query checked only after each complete logical paired update",
    }
    (out / "preregistered_design.json").write_text(
        json.dumps(design, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    graph_records = []
    cell_records = []
    for graph_seed in GRAPH_SEEDS:
        edges = preferential_attachment(graph_seed)
        graph_path = graphs_dir / f"ba-n{N}-m{ATTACHMENTS}-seed-{graph_seed}.gr.gz"
        write_symmetric_dimacs(graph_path, edges)
        graph_sha = sha256_file(graph_path)
        graph_record = {
            "graph_seed": graph_seed,
            "path": str(graph_path),
            "graph_sha256": graph_sha,
            "vertices": N,
            "logical_undirected_edges": len(edges),
            "ades_directed_arcs": 2 * len(edges),
        }
        graph_records.append(graph_record)

        for family in FAMILIES:
            for workload_seed in WORKLOAD_SEEDS:
                cell_id = f"ba-g{graph_seed}__{family['name']}__w{workload_seed}"
                cell_dir = cells_dir / cell_id
                trace_path = cell_dir / "trace.txt"
                meta = write_trace(trace_path, edges, graph_seed=graph_seed,
                                   workload_seed=workload_seed, family=family)
                meta.update({
                    "cell_id": cell_id,
                    "graph_path": str(graph_path),
                    "graph_sha256": graph_sha,
                    "trace_path": str(trace_path),
                })
                cell_dir.mkdir(parents=True, exist_ok=True)
                (cell_dir / "workload.json").write_text(
                    json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                cell_records.append(meta)

    manifest = {
        "schema": "ades-pr68-generated-manifest-v1",
        "design": design,
        "graphs": graph_records,
        "cells": cell_records,
        "cell_count": len(cell_records),
    }
    (out / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "graphs": len(graph_records),
        "cells": len(cell_records),
        "queries_per_cell": QUERY_COUNT,
        "logical_updates_per_cell": QUERY_COUNT // UPDATE_INTERVAL,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
