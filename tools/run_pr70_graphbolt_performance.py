#!/usr/bin/env python3
"""PR70 preregistered fixed-source GraphBolt vs ADES performance holdout.

The experiment contract is docs/PR70_GRAPHBOLT_FIXED_SOURCE_PERFORMANCE_CONTRACT.md.
Correctness is established in separate non-timed passes before any measured pair
is executed. Comparative outcome does not influence workload generation.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import heapq
import json
import math
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import time

N = 4096
M = 32768
REPLACEMENTS = 128
SEEDS = [7001, 7003, 7009, 7013]
MAX_DISTANCE = 65535
BOOTSTRAP_SEED = 70070
BOOTSTRAP_SAMPLES = 20000
GRAPHBOLT_SHA = "7c2792cc77d59b9722288ded8c3c62f6a9a87fc7"
MIMALLOC_SHA = "04f1c3b1e23677ce03bd16137e73089abd552175"
PATCH_ID = "kickstarter-missing-return-v1"
GRAPHBOLT_TIMEOUT_S = 180
ADES_TIMEOUT_S = 180


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def make_graph(seed: int) -> dict[tuple[int, int], int]:
    rng = random.Random(seed)
    edges: dict[tuple[int, int], int] = {}
    for u in range(N):
        edges[(u, (u + 1) % N)] = rng.randint(1, 30)
    while len(edges) < M:
        u = rng.randrange(N)
        v = rng.randrange(N)
        if u == v or (u, v) in edges:
            continue
        edges[(u, v)] = rng.randint(1, 30)
    return edges


def make_updates(seed: int, edges: dict[tuple[int, int], int], ordered_keys: list[tuple[int, int]]):
    rng = random.Random(seed ^ 0x70555F70)
    current = [edges[k] for k in ordered_keys]
    updates: list[tuple[int, int, int]] = []
    for i in range(REPLACEMENTS):
        if i % 2 == 0:
            candidates = [idx for idx, w in enumerate(current) if w > 1]
            if not candidates:
                raise AssertionError("no edge eligible for preregistered decrease")
            edge_id = candidates[rng.randrange(len(candidates))]
            old = current[edge_id]
            delta = rng.randint(1, min(7, old - 1))
            new = old - delta
            if not new < old:
                raise AssertionError("decrease schedule defect")
        else:
            edge_id = rng.randrange(len(current))
            old = current[edge_id]
            new = old + rng.randint(1, 7)
            if not new > old:
                raise AssertionError("increase schedule defect")
        current[edge_id] = new
        updates.append((edge_id, old, new))
    return updates


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
                raise RuntimeError("GraphBolt uint16 distance-bound violation")
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(pq, (nd, v))
    return dist


def max_finite(dist: list[int]) -> int:
    return max((d for d in dist if d < MAX_DISTANCE), default=0)


def write_graphbolt_adjacency(path: Path, edges: dict[tuple[int, int], int]) -> None:
    by_src: list[list[tuple[int, int]]] = [[] for _ in range(N)]
    for (u, v), w in sorted(edges.items()):
        by_src[u].append((v, w))
    offsets, destinations, weights = [], [], []
    offset = 0
    for u in range(N):
        offsets.append(offset)
        for v, w in by_src[u]:
            destinations.append(v)
            weights.append(w)
            offset += 1
    if len(destinations) != M:
        raise AssertionError("GraphBolt edge-count mismatch")
    tokens = ["WeightedAdjacencyGraph", str(N), str(M)]
    tokens.extend(str(x) for x in offsets)
    tokens.extend(str(x) for x in destinations)
    tokens.extend(str(x) for x in weights)
    path.write_text("\n".join(tokens) + "\n", encoding="ascii")


def write_case(seed: int, root: Path) -> dict:
    case = root / f"seed-{seed}"
    case.mkdir(parents=True, exist_ok=True)
    source = seed % N
    initial = make_graph(seed)
    ordered_keys = list(sorted(initial))
    edge_id_for = {key: i for i, key in enumerate(ordered_keys)}
    updates = make_updates(seed, initial, ordered_keys)

    graph_tsv = case / "graph.tsv"
    with graph_tsv.open("w", encoding="ascii") as f:
        f.write(f"{N} {M} {source}\n")
        for edge_id, (u, v) in enumerate(ordered_keys):
            f.write(f"{edge_id} {u} {v} {initial[(u, v)]}\n")

    updates_tsv = case / "updates.tsv"
    with updates_tsv.open("w", encoding="ascii") as f:
        f.write(f"{len(updates)}\n")
        for edge_id, old, new in updates:
            f.write(f"{edge_id} {old} {new}\n")

    graphbolt_adj = case / "initial.adj"
    write_graphbolt_adjacency(graphbolt_adj, initial)
    stream = case / "stream.txt"
    with stream.open("w", encoding="ascii") as f:
        for edge_id, old, new in updates:
            u, v = ordered_keys[edge_id]
            f.write(f"d {u} {v} {old}\n")
            f.write(f"a {u} {v} {new}\n")

    # Freeze and validate the full logical state sequence before any comparator is run.
    state = dict(initial)
    max_distance = max_finite(dijkstra(state, source))
    for edge_id, old, new in updates:
        key = ordered_keys[edge_id]
        if state[key] != old:
            raise AssertionError("update state drift during preregistration")
        state[key] = new
        max_distance = max(max_distance, max_finite(dijkstra(state, source)))
    if max_distance >= MAX_DISTANCE:
        raise RuntimeError(f"seed {seed} violates GraphBolt uint16 distance boundary")

    manifest = {
        "seed": seed,
        "source": source,
        "vertices": N,
        "initial_edges": M,
        "logical_replacements": REPLACEMENTS,
        "stream_batches": 2 * REPLACEMENTS,
        "max_finite_oracle_distance": max_distance,
        "graph_tsv_sha256": sha256_file(graph_tsv),
        "updates_tsv_sha256": sha256_file(updates_tsv),
        "graphbolt_adjacency_sha256": sha256_file(graphbolt_adj),
        "graphbolt_stream_sha256": sha256_file(stream),
    }
    (case / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return {
        "dir": case,
        "source": source,
        "initial": initial,
        "ordered_keys": ordered_keys,
        "updates": updates,
        "manifest": manifest,
    }


def parse_graphbolt_output(path: Path) -> list[int]:
    if not path.exists():
        raise RuntimeError(f"missing GraphBolt output {path}")
    dist: list[int | None] = [None] * N
    with path.open("r", encoding="utf-8") as f:
        for raw in f:
            parts = raw.split()
            if len(parts) < 4:
                raise RuntimeError(f"malformed GraphBolt output line: {raw!r}")
            v = int(parts[0])
            value = int(parts[-1])
            if not 0 <= v < N:
                raise RuntimeError("GraphBolt vertex out of range")
            dist[v] = value
    if any(x is None for x in dist):
        raise RuntimeError("GraphBolt output missing vertices")
    return [int(x) for x in dist]


def first_mismatch(got: list[int], expected: list[int]):
    for v, (a, b) in enumerate(zip(got, expected)):
        if a != b:
            return {"vertex": v, "graphbolt": a, "fresh_dijkstra": b}
    return None


def graphbolt_cmd(exe: str, case: dict, output_prefix: Path | None = None):
    cmd = [
        exe,
        "-source", str(case["source"]),
        "-numberOfUpdateBatches", str(2 * REPLACEMENTS),
        "-nEdges", "1",
        "-fixedBatchSize",
        "-simple",
        "-enforceEdgeValidity",
        "-streamPath", str(case["dir"] / "stream.txt"),
    ]
    if output_prefix is not None:
        cmd.extend(["-outputFile", str(output_prefix)])
    cmd.append(str(case["dir"] / "initial.adj"))
    return cmd


def run_process(cmd: list[str], env: dict[str, str], timeout: int):
    start = time.perf_counter()
    p = subprocess.run(cmd, text=True, capture_output=True, env=env, timeout=timeout)
    wall = time.perf_counter() - start
    return p, wall


def verify_graphbolt(exe: str, case: dict, env: dict[str, str]) -> dict:
    out_dir = case["dir"] / "graphbolt-correctness-output"
    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir()
    prefix = out_dir / "out-"
    p, wall = run_process(graphbolt_cmd(exe, case, prefix), env, GRAPHBOLT_TIMEOUT_S)
    if p.returncode != 0:
        shutil.rmtree(out_dir, ignore_errors=True)
        return {"exact": False, "failure": "NONZERO_EXIT", "returncode": p.returncode,
                "stdout_tail": p.stdout[-4000:], "stderr_tail": p.stderr[-4000:], "wall_s": wall}

    state = dict(case["initial"])
    checks = 0
    expected = dijkstra(state, case["source"])
    mismatch = first_mismatch(parse_graphbolt_output(Path(f"{prefix}0")), expected)
    checks += 1
    if mismatch:
        shutil.rmtree(out_dir, ignore_errors=True)
        return {"exact": False, "checks": checks, "phase": "initial", "mismatch": mismatch}

    for i, (edge_id, old, new) in enumerate(case["updates"]):
        key = case["ordered_keys"][edge_id]
        if state[key] != old:
            raise AssertionError("GraphBolt correctness oracle state drift")
        state[key] = new
        expected = dijkstra(state, case["source"])
        got = parse_graphbolt_output(Path(f"{prefix}{2 * (i + 1)}"))
        checks += 1
        mismatch = first_mismatch(got, expected)
        if mismatch:
            shutil.rmtree(out_dir, ignore_errors=True)
            return {"exact": False, "checks": checks, "phase": "after_logical_replacement",
                    "replacement": i, "edge_id": edge_id, "old": old, "new": new,
                    "mismatch": mismatch}
    shutil.rmtree(out_dir, ignore_errors=True)
    return {"exact": True, "checks": checks, "wall_s": wall}


def parse_key_values(text: str) -> dict[str, str]:
    out = {}
    for line in text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def verify_ades(exe: str, case: dict, env: dict[str, str]) -> dict:
    cmd = [exe, str(case["dir"] / "graph.tsv"), str(case["dir"] / "updates.tsv"), "exact"]
    p, wall = run_process(cmd, env, ADES_TIMEOUT_S)
    kv = parse_key_values(p.stdout)
    return {
        "exact": p.returncode == 0 and kv.get("exact") == "1",
        "returncode": p.returncode,
        "checks": 1 + REPLACEMENTS if p.returncode == 0 and kv.get("exact") == "1" else None,
        "max_finite_distance": int(kv.get("max_finite_distance", "-1")),
        "first_mismatch_update": int(kv.get("first_mismatch_update", "-1")),
        "first_mismatch_vertex": int(kv.get("first_mismatch_vertex", "-1")),
        "wall_s": wall,
        "stderr_tail": p.stderr[-4000:],
    }


_INITIAL_RE = re.compile(r"Initial graph processing\s*:\s*([0-9eE+\-.]+)")
_BATCH_RE = re.compile(r"Finished batch\s*:\s*([0-9eE+\-.]+)")


def timed_graphbolt(exe: str, case: dict, env: dict[str, str]) -> dict:
    p, wall = run_process(graphbolt_cmd(exe, case), env, GRAPHBOLT_TIMEOUT_S)
    if p.returncode != 0:
        raise RuntimeError(f"GraphBolt timed run failed rc={p.returncode}: {p.stderr[-4000:]}")
    init = _INITIAL_RE.findall(p.stdout)
    batches = [float(x) for x in _BATCH_RE.findall(p.stdout)]
    if len(init) != 1 or len(batches) != 2 * REPLACEMENTS:
        raise RuntimeError(f"malformed GraphBolt timing output init={len(init)} batches={len(batches)}")
    init_ns = int(round(float(init[0]) * 1e9))
    maintenance_ns = int(round(sum(batches) * 1e9))
    return {
        "implementation": "GraphBolt-portability-patched-OMP4",
        "initialization_ns": init_ns,
        "maintenance_ns": maintenance_ns,
        "total_ns": init_ns + maintenance_ns,
        "process_wall_s": wall,
        "stream_batches": len(batches),
        "rebuilds": "",
        "repaired": "",
        "filtered": "",
    }


def timed_ades(exe: str, case: dict, env: dict[str, str]) -> dict:
    cmd = [exe, str(case["dir"] / "graph.tsv"), str(case["dir"] / "updates.tsv"), "timed"]
    p, wall = run_process(cmd, env, ADES_TIMEOUT_S)
    if p.returncode != 0:
        raise RuntimeError(f"ADES timed run failed rc={p.returncode}: {p.stderr[-4000:]}")
    kv = parse_key_values(p.stdout)
    if kv.get("schema") != "ades-pr70-fixed-source-runner-v1" or kv.get("exact") != "1":
        raise RuntimeError("malformed ADES timing output")
    return {
        "implementation": "ADES-resident-fixed-source-core",
        "initialization_ns": int(kv["initialization_ns"]),
        "maintenance_ns": int(kv["maintenance_ns"]),
        "total_ns": int(kv["total_ns"]),
        "process_wall_s": wall,
        "stream_batches": REPLACEMENTS,
        "rebuilds": int(kv["rebuilds"]),
        "repaired": int(kv["repaired"]),
        "filtered": int(kv["filtered"]),
    }


def geomean(values: list[float]) -> float:
    if not values or any(v <= 0 for v in values):
        raise ValueError("geomean requires positive values")
    return math.exp(sum(math.log(v) for v in values) / len(values))


def bootstrap_ci(seed_values: list[float]) -> tuple[float, float]:
    rng = random.Random(BOOTSTRAP_SEED)
    draws = []
    n = len(seed_values)
    for _ in range(BOOTSTRAP_SAMPLES):
        sample = [seed_values[rng.randrange(n)] for _ in range(n)]
        draws.append(geomean(sample))
    draws.sort()
    lo = draws[int(0.025 * len(draws))]
    hi = draws[min(len(draws) - 1, int(0.975 * len(draws)))]
    return lo, hi


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--graphbolt", required=True)
    ap.add_argument("--ades", required=True)
    ap.add_argument("--mimalloc", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    out = Path(args.out)
    cases_root = out / "cases"
    cases_root.mkdir(parents=True, exist_ok=True)

    case_list = [write_case(seed, cases_root) for seed in SEEDS]
    case_manifests = [c["manifest"] for c in case_list]
    (out / "case-manifest.json").write_text(json.dumps(case_manifests, indent=2, sort_keys=True) + "\n")

    base_env = os.environ.copy()
    graphbolt_env = dict(base_env)
    graphbolt_env["OMP_NUM_THREADS"] = "4"
    graphbolt_env["LD_PRELOAD"] = str(Path(args.mimalloc).resolve())

    correctness = []
    for case in case_list:
        seed = case["manifest"]["seed"]
        try:
            gb = verify_graphbolt(args.graphbolt, case, graphbolt_env)
        except subprocess.TimeoutExpired as exc:
            gb = {"exact": False, "failure": "TIMEOUT", "timeout_s": GRAPHBOLT_TIMEOUT_S,
                  "stdout_tail": str(exc.stdout)[-4000:], "stderr_tail": str(exc.stderr)[-4000:]}
        try:
            ad = verify_ades(args.ades, case, base_env)
        except subprocess.TimeoutExpired as exc:
            ad = {"exact": False, "failure": "TIMEOUT", "timeout_s": ADES_TIMEOUT_S,
                  "stdout_tail": str(exc.stdout)[-4000:], "stderr_tail": str(exc.stderr)[-4000:]}
        row = {"seed": seed, "graphbolt": gb, "ades": ad,
               "distance_bound_ok": case["manifest"]["max_finite_oracle_distance"] < MAX_DISTANCE}
        correctness.append(row)

    all_exact = all(r["graphbolt"].get("exact") and r["ades"].get("exact") and r["distance_bound_ok"]
                    for r in correctness)
    (out / "correctness.json").write_text(json.dumps({
        "all_exact": all_exact,
        "seed_count": len(correctness),
        "cases": correctness,
    }, indent=2, sort_keys=True) + "\n")
    if not all_exact:
        (out / "summary.json").write_text(json.dumps({
            "disposition": "REJECTED_CORRECTNESS_OR_DISTANCE_BOUND",
            "performance_admissible": False,
            "correctness": correctness,
        }, indent=2, sort_keys=True) + "\n")
        print("PR70 correctness gate failed; comparative timing not executed")
        return 1

    rows = []
    pair_records = []
    for case in case_list:
        seed = case["manifest"]["seed"]
        for repeat in (0, 1):
            order = ["graphbolt", "ades"] if repeat == 0 else ["ades", "graphbolt"]
            measured = {}
            for impl in order:
                if impl == "graphbolt":
                    measured[impl] = timed_graphbolt(args.graphbolt, case, graphbolt_env)
                else:
                    measured[impl] = timed_ades(args.ades, case, base_env)
                rows.append({
                    "seed": seed,
                    "repeat": repeat,
                    "execution_order": "GB->ADES" if repeat == 0 else "ADES->GB",
                    **measured[impl],
                })
            gb, ad = measured["graphbolt"], measured["ades"]
            pair_records.append({
                "seed": seed,
                "repeat": repeat,
                "execution_order": "GB->ADES" if repeat == 0 else "ADES->GB",
                "maintenance_ratio_graphbolt_over_ades": gb["maintenance_ns"] / ad["maintenance_ns"],
                "total_ratio_graphbolt_over_ades": gb["total_ns"] / ad["total_ns"],
            })

    fields = ["seed", "repeat", "execution_order", "implementation", "initialization_ns",
              "maintenance_ns", "total_ns", "process_wall_s", "stream_batches",
              "rebuilds", "repaired", "filtered"]
    with (out / "measurements.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    with (out / "paired-ratios.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(pair_records[0]))
        w.writeheader()
        w.writerows(pair_records)

    seed_stats = []
    for seed in SEEDS:
        pairs = [p for p in pair_records if p["seed"] == seed]
        maint = geomean([p["maintenance_ratio_graphbolt_over_ades"] for p in pairs])
        total = geomean([p["total_ratio_graphbolt_over_ades"] for p in pairs])
        seed_stats.append({"seed": seed, "maintenance_ratio": maint, "total_ratio": total})

    maintenance_values = [x["maintenance_ratio"] for x in seed_stats]
    total_values = [x["total_ratio"] for x in seed_stats]
    maintenance_gm = geomean(maintenance_values)
    total_gm = geomean(total_values)
    maintenance_ci = bootstrap_ci(maintenance_values)
    total_ci = bootstrap_ci(total_values)

    supports = (
        all_exact
        and len(pair_records) == 8
        and maintenance_gm > 1.0 and maintenance_ci[0] > 1.0
        and total_gm > 1.0 and total_ci[0] > 1.0
        and all(x > 1.0 for x in maintenance_values)
    )
    disposition = (
        "SUPPORTS_ADES_RESIDENT_FIXED_SOURCE_CORE_SUPERIORITY_ON_PR70_TESTED_SLICE"
        if supports else
        "FALSIFIES_PREFERRED_PR70_SUPERIORITY_HYPOTHESIS_ON_TESTED_SLICE"
    )
    summary = {
        "schema": "ades-pr70-graphbolt-fixed-source-performance-v1",
        "contract": "docs/PR70_GRAPHBOLT_FIXED_SOURCE_PERFORMANCE_CONTRACT.md",
        "graphbolt_commit": GRAPHBOLT_SHA,
        "mimalloc_commit": MIMALLOC_SHA,
        "portability_patch_id": PATCH_ID,
        "graphbolt_threads": 4,
        "correctness_all_exact": all_exact,
        "independent_seeds": len(SEEDS),
        "measured_pairs": len(pair_records),
        "seed_level": seed_stats,
        "maintenance": {
            "geomean_graphbolt_over_ades": maintenance_gm,
            "bootstrap95_low": maintenance_ci[0],
            "bootstrap95_high": maintenance_ci[1],
        },
        "total": {
            "geomean_graphbolt_over_ades": total_gm,
            "bootstrap95_low": total_ci[0],
            "bootstrap95_high": total_ci[1],
        },
        "bootstrap_seed": BOOTSTRAP_SEED,
        "bootstrap_samples": BOOTSTRAP_SAMPLES,
        "disposition": disposition,
        "claim_boundary": "resident fixed-source dynamic SSSP maintenance core on the frozen synthetic directed PR70 slice only",
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
