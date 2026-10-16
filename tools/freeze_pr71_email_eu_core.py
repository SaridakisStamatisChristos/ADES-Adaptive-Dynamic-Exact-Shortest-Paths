#!/usr/bin/env python3
"""Freeze and structurally validate SNAP email-Eu-core for PR71.

No ADES or comparator execution occurs here. The script validates the external
real directed topology against preregistered SNAP structural invariants and
emits a canonical sorted edge list plus cryptographic hashes.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil

SOURCE_URL = "https://snap.stanford.edu/data/email-Eu-core.txt.gz"
EXPECTED_NODES = 1005
EXPECTED_EDGES = 25571
EXPECTED_WCC_NODES = 986
EXPECTED_WCC_EDGES = 25552
EXPECTED_SCC_NODES = 803
EXPECTED_SCC_EDGES = 24729
EXPECTED_MIN_ID = 0
EXPECTED_MAX_ID = 1004
PASS = "QUALIFIED_EXTERNAL_DIRECTED_TOPOLOGY_EMAIL_EU_CORE"
FAIL = "REJECTED_EXTERNAL_TOPOLOGY_FREEZE"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_edges(path: Path) -> list[tuple[int, int]]:
    edges: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()
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
                raise ValueError(f"negative node id at line {lineno}")
            if u == v:
                raise ValueError(f"self-loop at line {lineno}: {(u, v)}")
            e = (u, v)
            if e in seen:
                raise ValueError(f"duplicate directed edge at line {lineno}: {e}")
            seen.add(e)
            edges.append(e)
    return edges


def component_nodes(adj: list[list[int]]) -> list[set[int]]:
    n = len(adj)
    seen = [False] * n
    comps: list[set[int]] = []
    for root in range(n):
        if seen[root]:
            continue
        stack = [root]
        seen[root] = True
        comp: set[int] = set()
        while stack:
            u = stack.pop()
            comp.add(u)
            for v in adj[u]:
                if not seen[v]:
                    seen[v] = True
                    stack.append(v)
        comps.append(comp)
    return comps


def kosaraju_scc(n: int, edges: list[tuple[int, int]]) -> list[set[int]]:
    adj = [[] for _ in range(n)]
    rev = [[] for _ in range(n)]
    for u, v in edges:
        adj[u].append(v)
        rev[v].append(u)

    seen = [False] * n
    order: list[int] = []
    for root in range(n):
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

    seen = [False] * n
    comps: list[set[int]] = []
    for root in reversed(order):
        if seen[root]:
            continue
        comp: set[int] = set()
        stack = [root]
        seen[root] = True
        while stack:
            u = stack.pop()
            comp.add(u)
            for v in rev[u]:
                if not seen[v]:
                    seen[v] = True
                    stack.append(v)
        comps.append(comp)
    return comps


def internal_edge_count(edges: list[tuple[int, int]], comp: set[int]) -> int:
    return sum(1 for u, v in edges if u in comp and v in comp)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-gz", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--expected-gzip-sha256", default="")
    ap.add_argument("--expected-canonical-sha256", default="")
    args = ap.parse_args()

    src = Path(args.input_gz)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    raw_copy = out / "email-Eu-core.txt.gz"
    if src.resolve() != raw_copy.resolve():
        shutil.copyfile(src, raw_copy)

    gzip_sha = sha256_file(raw_copy)
    edges = parse_edges(raw_copy)
    vertices = {x for e in edges for x in e}

    failures: list[str] = []
    checks = {
        "unique_nodes": len(vertices),
        "directed_edges": len(edges),
        "min_node_id": min(vertices) if vertices else None,
        "max_node_id": max(vertices) if vertices else None,
    }
    if len(vertices) != EXPECTED_NODES:
        failures.append(f"unique_nodes={len(vertices)} expected={EXPECTED_NODES}")
    if len(edges) != EXPECTED_EDGES:
        failures.append(f"directed_edges={len(edges)} expected={EXPECTED_EDGES}")
    if not vertices or min(vertices) != EXPECTED_MIN_ID or max(vertices) != EXPECTED_MAX_ID:
        failures.append("vertex id range mismatch")
    if vertices != set(range(EXPECTED_NODES)):
        failures.append("vertex id set is not exactly 0..1004")

    n = EXPECTED_NODES
    undirected = [[] for _ in range(n)]
    for u, v in edges:
        if u >= n or v >= n:
            failures.append(f"node id outside expected range: {(u, v)}")
            continue
        undirected[u].append(v)
        undirected[v].append(u)

    if not failures:
        wccs = component_nodes(undirected)
        largest_wcc = max(wccs, key=len)
        wcc_edges = internal_edge_count(edges, largest_wcc)
        sccs = kosaraju_scc(n, edges)
        largest_scc = max(sccs, key=len)
        scc_edges = internal_edge_count(edges, largest_scc)
        checks.update({
            "largest_wcc_nodes": len(largest_wcc),
            "largest_wcc_edges": wcc_edges,
            "largest_scc_nodes": len(largest_scc),
            "largest_scc_edges": scc_edges,
        })
        if len(largest_wcc) != EXPECTED_WCC_NODES:
            failures.append(f"largest_wcc_nodes={len(largest_wcc)} expected={EXPECTED_WCC_NODES}")
        if wcc_edges != EXPECTED_WCC_EDGES:
            failures.append(f"largest_wcc_edges={wcc_edges} expected={EXPECTED_WCC_EDGES}")
        if len(largest_scc) != EXPECTED_SCC_NODES:
            failures.append(f"largest_scc_nodes={len(largest_scc)} expected={EXPECTED_SCC_NODES}")
        if scc_edges != EXPECTED_SCC_EDGES:
            failures.append(f"largest_scc_edges={scc_edges} expected={EXPECTED_SCC_EDGES}")

    canonical = out / "email-Eu-core.canonical.edgelist"
    canonical.write_text("".join(f"{u} {v}\n" for u, v in sorted(edges)), encoding="ascii")
    canonical_sha = sha256_file(canonical)

    if args.expected_gzip_sha256 and gzip_sha != args.expected_gzip_sha256:
        failures.append("gzip SHA-256 does not match frozen expected hash")
    if args.expected_canonical_sha256 and canonical_sha != args.expected_canonical_sha256:
        failures.append("canonical edge-list SHA-256 does not match frozen expected hash")

    result = {
        "schema": "ades-pr71-email-eu-core-topology-freeze-v1",
        "source_name": "SNAP email-Eu-core",
        "source_url": SOURCE_URL,
        "source_semantics": "directed email communication topology; unweighted source artifact",
        "gzip_sha256": gzip_sha,
        "canonical_edge_list_sha256": canonical_sha,
        "parser_rules": {
            "comment_prefixes": ["#", "%"],
            "data_columns": 2,
            "direction_preserved": True,
            "self_loops_allowed": False,
            "duplicate_directed_edges_allowed": False,
            "canonical_order": "lexicographic (u,v)",
        },
        "expected": {
            "unique_nodes": EXPECTED_NODES,
            "directed_edges": EXPECTED_EDGES,
            "min_node_id": EXPECTED_MIN_ID,
            "max_node_id": EXPECTED_MAX_ID,
            "largest_wcc_nodes": EXPECTED_WCC_NODES,
            "largest_wcc_edges": EXPECTED_WCC_EDGES,
            "largest_scc_nodes": EXPECTED_SCC_NODES,
            "largest_scc_edges": EXPECTED_SCC_EDGES,
        },
        "observed": checks,
        "expected_gzip_sha256": args.expected_gzip_sha256 or None,
        "expected_canonical_sha256": args.expected_canonical_sha256 or None,
        "hashes_frozen_in_invocation": bool(args.expected_gzip_sha256 and args.expected_canonical_sha256),
        "failures": failures,
        "disposition": PASS if not failures else FAIL,
        "claim_boundary": "external directed topology identity/provenance only; no performance or native weight claim",
    }
    (out / "manifest.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
