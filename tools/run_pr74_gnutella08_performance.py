#!/usr/bin/env python3
"""PR74 preregistered GraphBolt vs ADES holdout on SNAP p2p-Gnutella08."""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import random
import subprocess

import run_pr70_graphbolt_performance as core

N = 6301
M = 20777
REPLACEMENTS = 128
MAX_DISTANCE = 65535
TOPOLOGY_GZIP_SHA256 = "cb3019062be21e55b03f19a73e88edf34693bafb68ae581621cc7a037fa604a9"
TOPOLOGY_CANONICAL_SHA256 = "4554021f32967c69b68ccf59f9d85b83b66a6c0413b721112a10741858fdf845"
SCENARIOS = [
    {"scenario": "S0", "source": 617, "weight_seed": 7401},
    {"scenario": "S1", "source": 2079, "weight_seed": 7407},
    {"scenario": "S2", "source": 3676, "weight_seed": 7411},
    {"scenario": "S3", "source": 5306, "weight_seed": 7417},
]
EXPECTED_SOURCES = [617, 2079, 3676, 5306]
EXPECTED_SCC_SIZE = 2068
EXPECTED_POSITIONS = [258, 775, 1292, 1809]
BOOTSTRAP_SEED = 74074
BOOTSTRAP_SAMPLES = 20000
UPDATE_XOR = 0x74505550
EXPECTED_PREFLIGHT_MAX = {"S0": 217, "S1": 208, "S2": 212, "S3": 247}

core.N = N
core.M = M
core.REPLACEMENTS = REPLACEMENTS
core.MAX_DISTANCE = MAX_DISTANCE
core.BOOTSTRAP_SEED = BOOTSTRAP_SEED
core.BOOTSTRAP_SAMPLES = BOOTSTRAP_SAMPLES


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_topology(path: Path) -> list[tuple[int, int]]:
    if core.sha256_file(path) != TOPOLOGY_GZIP_SHA256:
        raise RuntimeError("PR73 raw topology gzip SHA-256 mismatch")
    rows: list[tuple[int, int]] = []
    with gzip.open(path, "rt", encoding="utf-8", newline=None) as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("%"):
                continue
            parts = line.split()
            if len(parts) != 2:
                raise RuntimeError(f"malformed topology row {lineno}: {raw!r}")
            u, v = int(parts[0]), int(parts[1])
            if u < 0 or v < 0:
                raise RuntimeError(f"negative node id at row {lineno}")
            rows.append((u, v))
    unique_edges = sorted(set(rows))
    if len(rows) != M or len(unique_edges) != M:
        raise RuntimeError("PR73 row/unique-edge count drift")
    if any(u == v for u, v in unique_edges):
        raise RuntimeError("PR73 self-loop invariant drift")
    nodes = {x for e in unique_edges for x in e}
    if nodes != set(range(N)):
        raise RuntimeError("PR73 node identity drift")
    canonical = "".join(f"{u} {v}\n" for u, v in unique_edges).encode("ascii")
    if sha256_bytes(canonical) != TOPOLOGY_CANONICAL_SHA256:
        raise RuntimeError("PR73 canonical topology SHA-256 mismatch")
    return unique_edges


def largest_scc(edges: list[tuple[int, int]]) -> list[int]:
    adj = [[] for _ in range(N)]
    rev = [[] for _ in range(N)]
    for u, v in edges:
        adj[u].append(v)
        rev[v].append(u)
    seen = [False] * N
    order: list[int] = []
    for root in range(N):
        if seen[root]:
            continue
        stack: list[tuple[int, int]] = [(root, 0)]
        seen[root] = True
        while stack:
            u, idx = stack[-1]
            if idx < len(adj[u]):
                v = adj[u][idx]
                stack[-1] = (u, idx + 1)
                if not seen[v]:
                    seen[v] = True
                    stack.append((v, 0))
            else:
                order.append(u)
                stack.pop()
    seen = [False] * N
    comps: list[list[int]] = []
    for root in reversed(order):
        if seen[root]:
            continue
        comp: list[int] = []
        stack = [root]
        seen[root] = True
        while stack:
            u = stack.pop()
            comp.append(u)
            for v in rev[u]:
                if not seen[v]:
                    seen[v] = True
                    stack.append(v)
        comps.append(comp)
    return sorted(max(comps, key=len))


def validate_frozen_sources(edges: list[tuple[int, int]]) -> dict:
    scc = largest_scc(edges)
    if len(scc) != EXPECTED_SCC_SIZE:
        raise RuntimeError(f"largest SCC size drift: {len(scc)} != {EXPECTED_SCC_SIZE}")
    selected = [scc[i] for i in EXPECTED_POSITIONS]
    if selected != EXPECTED_SOURCES:
        raise RuntimeError(f"frozen SCC quantile sources drifted: {selected}")
    return {"largest_scc_nodes": len(scc), "positions": EXPECTED_POSITIONS, "sources": selected}


def stable_weight(seed: int, u: int, v: int) -> int:
    payload = f"pr74:{seed}:{u}:{v}".encode("ascii")
    value = int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")
    return 1 + value % 30


def make_updates(seed: int, initial: dict[tuple[int, int], int], ordered_keys: list[tuple[int, int]]):
    rng = random.Random(seed ^ UPDATE_XOR)
    current = [initial[k] for k in ordered_keys]
    updates: list[tuple[int, int, int]] = []
    for i in range(REPLACEMENTS):
        if i % 2 == 0:
            candidates = [idx for idx, w in enumerate(current) if w > 1]
            if not candidates:
                raise AssertionError("no edge eligible for decrease")
            edge_id = candidates[rng.randrange(len(candidates))]
            old = current[edge_id]
            new = old - rng.randint(1, min(7, old - 1))
        else:
            edge_id = rng.randrange(len(ordered_keys))
            old = current[edge_id]
            new = old + rng.randint(1, 7)
        if new == old:
            raise AssertionError("non-strict replacement")
        current[edge_id] = new
        updates.append((edge_id, old, new))
    return updates


def write_case(spec: dict, topology: list[tuple[int, int]], root: Path) -> dict:
    label = spec["scenario"]
    source = spec["source"]
    seed = spec["weight_seed"]
    case = root / label
    case.mkdir(parents=True, exist_ok=True)
    initial = {e: stable_weight(seed, e[0], e[1]) for e in topology}
    ordered_keys = list(topology)
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
    core.write_graphbolt_adjacency(graphbolt_adj, initial)
    stream = case / "stream.txt"
    with stream.open("w", encoding="ascii") as f:
        for edge_id, old, new in updates:
            u, v = ordered_keys[edge_id]
            f.write(f"d {u} {v} {old}\n")
            f.write(f"a {u} {v} {new}\n")

    state = dict(initial)
    max_distance = core.max_finite(core.dijkstra(state, source))
    for edge_id, old, new in updates:
        key = ordered_keys[edge_id]
        if state[key] != old:
            raise AssertionError("workload state drift")
        state[key] = new
        max_distance = max(max_distance, core.max_finite(core.dijkstra(state, source)))
    if max_distance >= MAX_DISTANCE:
        raise RuntimeError(f"{label} violates GraphBolt uint16 distance boundary")
    if max_distance != EXPECTED_PREFLIGHT_MAX[label]:
        raise RuntimeError(f"{label} preflight max-distance drift: {max_distance} != {EXPECTED_PREFLIGHT_MAX[label]}")

    manifest = {
        "scenario": label, "source": source, "weight_seed": seed,
        "vertices": N, "initial_edges": M, "logical_replacements": REPLACEMENTS,
        "stream_batches": 2 * REPLACEMENTS, "max_finite_oracle_distance": max_distance,
        "topology_gzip_sha256": TOPOLOGY_GZIP_SHA256,
        "topology_canonical_sha256": TOPOLOGY_CANONICAL_SHA256,
        "graph_tsv_sha256": core.sha256_file(graph_tsv),
        "updates_tsv_sha256": core.sha256_file(updates_tsv),
        "graphbolt_adjacency_sha256": core.sha256_file(graphbolt_adj),
        "graphbolt_stream_sha256": core.sha256_file(stream),
    }
    (case / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return {"dir": case, "source": source, "initial": initial, "ordered_keys": ordered_keys,
            "updates": updates, "manifest": manifest}


def geomean(values: list[float]) -> float:
    if not values or any(v <= 0 for v in values):
        raise ValueError("geomean requires positive values")
    return math.exp(sum(math.log(v) for v in values) / len(values))


def bootstrap_ci(values: list[float]) -> tuple[float, float]:
    rng = random.Random(BOOTSTRAP_SEED)
    draws = []
    n = len(values)
    for _ in range(BOOTSTRAP_SAMPLES):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        draws.append(geomean(sample))
    draws.sort()
    return draws[int(0.025 * len(draws))], draws[min(len(draws) - 1, int(0.975 * len(draws)))]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--graphbolt", required=True)
    ap.add_argument("--ades", required=True)
    ap.add_argument("--mimalloc", required=True)
    ap.add_argument("--topology-gz", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    out = Path(args.out)
    cases_root = out / "cases"
    cases_root.mkdir(parents=True, exist_ok=True)

    topology = parse_topology(Path(args.topology_gz))
    source_validation = validate_frozen_sources(topology)
    topology_validation = {
        "raw_gzip_sha256": core.sha256_file(Path(args.topology_gz)),
        "canonical_sha256": TOPOLOGY_CANONICAL_SHA256,
        "vertices": N, "directed_edges": M, "self_loops": 0, "duplicate_rows": 0,
        "largest_scc": source_validation,
        "synthetic_weight_semantics": True, "graph_transform_applied": False,
    }
    (out / "topology-validation.json").write_text(json.dumps(topology_validation, indent=2, sort_keys=True) + "\n")
    cases = [write_case(spec, topology, cases_root) for spec in SCENARIOS]
    (out / "case-manifest.json").write_text(json.dumps([c["manifest"] for c in cases], indent=2, sort_keys=True) + "\n")

    base_env = os.environ.copy()
    graphbolt_env = dict(base_env)
    graphbolt_env["OMP_NUM_THREADS"] = "4"
    graphbolt_env["LD_PRELOAD"] = str(Path(args.mimalloc).resolve())

    correctness = []
    for case in cases:
        label = case["manifest"]["scenario"]
        try:
            gb = core.verify_graphbolt(args.graphbolt, case, graphbolt_env)
        except subprocess.TimeoutExpired as exc:
            gb = {"exact": False, "failure": "TIMEOUT", "timeout_s": core.GRAPHBOLT_TIMEOUT_S,
                  "stdout_tail": str(exc.stdout)[-4000:], "stderr_tail": str(exc.stderr)[-4000:]}
        try:
            ad = core.verify_ades(args.ades, case, base_env)
        except subprocess.TimeoutExpired as exc:
            ad = {"exact": False, "failure": "TIMEOUT", "timeout_s": core.ADES_TIMEOUT_S,
                  "stdout_tail": str(exc.stdout)[-4000:], "stderr_tail": str(exc.stderr)[-4000:]}
        correctness.append({"scenario": label, "source": case["source"],
                            "weight_seed": case["manifest"]["weight_seed"],
                            "graphbolt": gb, "ades": ad,
                            "distance_bound_ok": case["manifest"]["max_finite_oracle_distance"] < MAX_DISTANCE})

    all_exact = all(r["graphbolt"].get("exact") and r["ades"].get("exact") and r["distance_bound_ok"]
                    for r in correctness)
    (out / "correctness.json").write_text(json.dumps({"all_exact": all_exact,
        "scenario_count": len(correctness), "cases": correctness}, indent=2, sort_keys=True) + "\n")
    if not all_exact:
        summary = {"schema": "ades-pr74-graphbolt-gnutella08-performance-v1",
                   "disposition": "REJECTED_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND",
                   "performance_admissible": False, "topology": topology_validation,
                   "correctness": correctness}
        (out / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 1

    rows = []
    pairs = []
    for case in cases:
        label = case["manifest"]["scenario"]
        for repeat in (0, 1):
            order = ["graphbolt", "ades"] if repeat == 0 else ["ades", "graphbolt"]
            measured = {}
            for impl in order:
                measured[impl] = (core.timed_graphbolt(args.graphbolt, case, graphbolt_env)
                                  if impl == "graphbolt" else core.timed_ades(args.ades, case, base_env))
                rows.append({"scenario": label, "source": case["source"],
                             "weight_seed": case["manifest"]["weight_seed"], "repeat": repeat,
                             "execution_order": "GB->ADES" if repeat == 0 else "ADES->GB",
                             **measured[impl]})
            gb, ad = measured["graphbolt"], measured["ades"]
            pairs.append({"scenario": label, "source": case["source"],
                          "weight_seed": case["manifest"]["weight_seed"], "repeat": repeat,
                          "execution_order": "GB->ADES" if repeat == 0 else "ADES->GB",
                          "maintenance_ratio_graphbolt_over_ades": gb["maintenance_ns"] / ad["maintenance_ns"],
                          "total_ratio_graphbolt_over_ades": gb["total_ns"] / ad["total_ns"]})

    fields = ["scenario", "source", "weight_seed", "repeat", "execution_order", "implementation",
              "initialization_ns", "maintenance_ns", "total_ns", "process_wall_s", "stream_batches",
              "rebuilds", "repaired", "filtered"]
    with (out / "measurements.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
    with (out / "paired-ratios.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(pairs[0])); w.writeheader(); w.writerows(pairs)

    scenario_stats = []
    for spec in SCENARIOS:
        ps = [p for p in pairs if p["scenario"] == spec["scenario"]]
        scenario_stats.append({"scenario": spec["scenario"], "source": spec["source"],
            "weight_seed": spec["weight_seed"],
            "maintenance_ratio": geomean([p["maintenance_ratio_graphbolt_over_ades"] for p in ps]),
            "total_ratio": geomean([p["total_ratio_graphbolt_over_ades"] for p in ps])})
    maintenance_values = [x["maintenance_ratio"] for x in scenario_stats]
    total_values = [x["total_ratio"] for x in scenario_stats]
    maintenance_gm, total_gm = geomean(maintenance_values), geomean(total_values)
    maintenance_ci, total_ci = bootstrap_ci(maintenance_values), bootstrap_ci(total_values)
    supports = (len(pairs) == 8 and maintenance_gm > 1 and maintenance_ci[0] > 1
                and total_gm > 1 and total_ci[0] > 1 and all(x > 1 for x in maintenance_values))
    disposition = ("SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_ON_PR74_GNUTELLA08_SLICE" if supports else
                   "FALSIFIES_PREFERRED_PR74_GNUTELLA08_HYPOTHESIS_ON_TESTED_SLICE")
    summary = {
        "schema": "ades-pr74-graphbolt-gnutella08-performance-v1",
        "contract": "docs/PR74_GRAPHBOLT_GNUTELLA08_PERFORMANCE_CONTRACT.md",
        "graphbolt_commit": core.GRAPHBOLT_SHA, "mimalloc_commit": core.MIMALLOC_SHA,
        "portability_patch_id": core.PATCH_ID, "graphbolt_threads": 4,
        "topology": topology_validation, "correctness_all_exact": all_exact,
        "scenario_count": len(SCENARIOS), "measured_pairs": len(pairs),
        "scenario_level": scenario_stats,
        "maintenance": {"geomean_graphbolt_over_ades": maintenance_gm,
                        "scenario_bootstrap95_low": maintenance_ci[0],
                        "scenario_bootstrap95_high": maintenance_ci[1]},
        "total": {"geomean_graphbolt_over_ades": total_gm,
                  "scenario_bootstrap95_low": total_ci[0], "scenario_bootstrap95_high": total_ci[1]},
        "bootstrap_seed": BOOTSTRAP_SEED, "bootstrap_samples": BOOTSTRAP_SAMPLES,
        "bootstrap_interpretation": "within-topology scenario robustness only; scenarios are not independent topology replications",
        "disposition": disposition,
        "claim_boundary": "one frozen real external directed Gnutella topology with deterministic synthetic positive weights and four fixed SCC-source scenarios only",
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
