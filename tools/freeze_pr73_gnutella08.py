#!/usr/bin/env python3
"""PR73 source-only discovery/freeze for SNAP p2p-Gnutella08.

Stage A discovers exact source hashes/representation while requiring SNAP's
published structural graph invariants. Stage B uses the same parser with all
observed representation values and hashes hard-pinned. No ADES or comparator
execution occurs here.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil

SOURCE_URL = "https://snap.stanford.edu/data/p2p-Gnutella08.txt.gz"
EXPECTED_NODES = 6301
EXPECTED_EDGES = 20777
EXPECTED_WCC_NODES = 6299
EXPECTED_WCC_EDGES = 20776
EXPECTED_SCC_NODES = 2068
EXPECTED_SCC_EDGES = 9313
DISCOVERY = "DISCOVERY_STRUCTURALLY_VALID_NOT_CANONICALLY_FROZEN"
QUALIFIED = "QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_P2P_GNUTELLA08"
REJECTED = "REJECTED_EXTERNAL_TOPOLOGY_P2P_GNUTELLA08"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_rows(path: Path) -> tuple[list[tuple[int, int]], int]:
    rows: list[tuple[int, int]] = []
    with gzip.open(path, "rt", encoding="utf-8", newline=None) as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("%"):
                continue
            parts = line.split()
            if len(parts) != 2:
                raise ValueError(f"malformed data row at line {lineno}: {raw!r}")
            try:
                u, v = int(parts[0]), int(parts[1])
            except ValueError as exc:
                raise ValueError(f"non-integer data row at line {lineno}: {raw!r}") from exc
            if u < 0 or v < 0:
                raise ValueError(f"negative node id at line {lineno}: {(u, v)}")
            rows.append((u, v))
    return rows, len(rows)


def weak_components(nodes: list[int], edges: list[tuple[int, int]]) -> list[set[int]]:
    adj = {u: [] for u in nodes}
    for u, v in edges:
        adj[u].append(v)
        adj[v].append(u)
    seen: set[int] = set()
    comps: list[set[int]] = []
    for root in nodes:
        if root in seen:
            continue
        stack = [root]
        seen.add(root)
        comp: set[int] = set()
        while stack:
            u = stack.pop()
            comp.add(u)
            for v in adj[u]:
                if v not in seen:
                    seen.add(v)
                    stack.append(v)
        comps.append(comp)
    return comps


def strongly_connected_components(nodes: list[int], edges: list[tuple[int, int]]) -> list[set[int]]:
    adj = {u: [] for u in nodes}
    rev = {u: [] for u in nodes}
    for u, v in edges:
        adj[u].append(v)
        rev[v].append(u)

    seen: set[int] = set()
    order: list[int] = []
    for root in nodes:
        if root in seen:
            continue
        stack: list[tuple[int, int]] = [(root, 0)]
        seen.add(root)
        while stack:
            u, idx = stack[-1]
            if idx < len(adj[u]):
                v = adj[u][idx]
                stack[-1] = (u, idx + 1)
                if v not in seen:
                    seen.add(v)
                    stack.append((v, 0))
            else:
                order.append(u)
                stack.pop()

    seen.clear()
    comps: list[set[int]] = []
    for root in reversed(order):
        if root in seen:
            continue
        comp: set[int] = set()
        stack = [root]
        seen.add(root)
        while stack:
            u = stack.pop()
            comp.add(u)
            for v in rev[u]:
                if v not in seen:
                    seen.add(v)
                    stack.append(v)
        comps.append(comp)
    return comps


def internal_edge_count(edges: list[tuple[int, int]], comp: set[int]) -> int:
    return sum(1 for u, v in edges if u in comp and v in comp)


def optional_int(value: str) -> int | None:
    return None if value == "" else int(value)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-gz", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--expected-gzip-sha256", default="")
    ap.add_argument("--expected-canonical-sha256", default="")
    ap.add_argument("--expected-raw-rows", default="")
    ap.add_argument("--expected-duplicates", default="")
    ap.add_argument("--expected-self-loops", default="")
    ap.add_argument("--expected-min-id", default="")
    ap.add_argument("--expected-max-id", default="")
    args = ap.parse_args()

    src = Path(args.input_gz)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    raw_copy = out / "p2p-Gnutella08.txt.gz"
    if src.resolve() != raw_copy.resolve():
        shutil.copyfile(src, raw_copy)

    gzip_sha = sha256_file(raw_copy)
    rows, raw_rows = parse_rows(raw_copy)
    unique_edges = sorted(set(rows))
    duplicate_rows = raw_rows - len(unique_edges)
    self_loops = sum(1 for u, v in unique_edges if u == v)
    nodes = sorted({x for e in unique_edges for x in e})

    failures: list[str] = []
    observed = {
        "raw_data_rows": raw_rows,
        "unique_nodes": len(nodes),
        "unique_directed_edges": len(unique_edges),
        "duplicate_rows": duplicate_rows,
        "self_loops": self_loops,
        "min_node_id": min(nodes) if nodes else None,
        "max_node_id": max(nodes) if nodes else None,
    }

    if len(nodes) != EXPECTED_NODES:
        failures.append(f"unique_nodes={len(nodes)} expected={EXPECTED_NODES}")
    if len(unique_edges) != EXPECTED_EDGES:
        failures.append(f"unique_edges={len(unique_edges)} expected={EXPECTED_EDGES}")

    if nodes and len(unique_edges) == EXPECTED_EDGES and len(nodes) == EXPECTED_NODES:
        wcc = max(weak_components(nodes, unique_edges), key=len)
        scc = max(strongly_connected_components(nodes, unique_edges), key=len)
        observed.update({
            "largest_wcc_nodes": len(wcc),
            "largest_wcc_edges": internal_edge_count(unique_edges, wcc),
            "largest_scc_nodes": len(scc),
            "largest_scc_edges": internal_edge_count(unique_edges, scc),
        })
        if observed["largest_wcc_nodes"] != EXPECTED_WCC_NODES:
            failures.append(f"largest_wcc_nodes={observed['largest_wcc_nodes']} expected={EXPECTED_WCC_NODES}")
        if observed["largest_wcc_edges"] != EXPECTED_WCC_EDGES:
            failures.append(f"largest_wcc_edges={observed['largest_wcc_edges']} expected={EXPECTED_WCC_EDGES}")
        if observed["largest_scc_nodes"] != EXPECTED_SCC_NODES:
            failures.append(f"largest_scc_nodes={observed['largest_scc_nodes']} expected={EXPECTED_SCC_NODES}")
        if observed["largest_scc_edges"] != EXPECTED_SCC_EDGES:
            failures.append(f"largest_scc_edges={observed['largest_scc_edges']} expected={EXPECTED_SCC_EDGES}")

    canonical = out / "p2p-Gnutella08.canonical.unique.edgelist"
    canonical.write_text("".join(f"{u} {v}\n" for u, v in unique_edges), encoding="ascii")
    canonical_sha = sha256_file(canonical)

    pins = {
        "gzip_sha256": args.expected_gzip_sha256 or None,
        "canonical_sha256": args.expected_canonical_sha256 or None,
        "raw_rows": optional_int(args.expected_raw_rows),
        "duplicates": optional_int(args.expected_duplicates),
        "self_loops": optional_int(args.expected_self_loops),
        "min_id": optional_int(args.expected_min_id),
        "max_id": optional_int(args.expected_max_id),
    }
    all_pins_supplied = all(v is not None for v in pins.values())

    if pins["gzip_sha256"] is not None and gzip_sha != pins["gzip_sha256"]:
        failures.append("gzip SHA-256 mismatch")
    if pins["canonical_sha256"] is not None and canonical_sha != pins["canonical_sha256"]:
        failures.append("canonical unique-edge-list SHA-256 mismatch")
    for key, obs_key in [
        ("raw_rows", "raw_data_rows"),
        ("duplicates", "duplicate_rows"),
        ("self_loops", "self_loops"),
        ("min_id", "min_node_id"),
        ("max_id", "max_node_id"),
    ]:
        if pins[key] is not None and observed[obs_key] != pins[key]:
            failures.append(f"{obs_key}={observed[obs_key]} expected_pin={pins[key]}")

    if failures:
        disposition = REJECTED
    elif all_pins_supplied:
        disposition = QUALIFIED
    else:
        disposition = DISCOVERY

    result = {
        "schema": "ades-pr73-gnutella08-topology-freeze-v1",
        "stage": "canonical" if all_pins_supplied else "discovery",
        "source_name": "SNAP p2p-Gnutella08",
        "source_url": SOURCE_URL,
        "source_semantics": "directed Gnutella peer-to-peer host connection topology; unweighted source artifact",
        "gzip_sha256": gzip_sha,
        "canonical_unique_edge_list_sha256": canonical_sha,
        "canonical_semantics": "lexicographically sorted unique directed endpoint pairs; direction preserved; unique self-loops preserved",
        "published_expected": {
            "unique_nodes": EXPECTED_NODES,
            "unique_directed_edges": EXPECTED_EDGES,
            "largest_wcc_nodes": EXPECTED_WCC_NODES,
            "largest_wcc_edges": EXPECTED_WCC_EDGES,
            "largest_scc_nodes": EXPECTED_SCC_NODES,
            "largest_scc_edges": EXPECTED_SCC_EDGES,
        },
        "observed": observed,
        "frozen_pins": pins,
        "all_representation_pins_supplied": all_pins_supplied,
        "failures": failures,
        "disposition": disposition,
        "comparative_timing_executed": False,
        "claim_boundary": "external directed topology identity/provenance only; no weighted, shortest-path, or performance claim",
    }
    (out / "manifest.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if disposition in {DISCOVERY, QUALIFIED} else 1


if __name__ == "__main__":
    raise SystemExit(main())
