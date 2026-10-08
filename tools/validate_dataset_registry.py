#!/usr/bin/env python3
"""Validate PR47 dataset provenance, hashes, counts, and topology diversity."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def dimacs_counts(path: Path) -> tuple[int, int]:
    opener = gzip.open if path.suffix == ".gz" else open
    problem: tuple[int, int] | None = None
    arc_lines = 0
    with opener(path, "rt", encoding="ascii") as handle:
        for line in handle:
            if line.startswith("p "):
                fields = line.split()
                if len(fields) != 4 or fields[:2] != ["p", "sp"]:
                    raise ValueError(f"{path}: malformed DIMACS problem line")
                problem = (int(fields[2]), int(fields[3]))
            elif line.startswith("a "):
                arc_lines += 1
    if problem is None:
        raise ValueError(f"{path}: missing DIMACS problem line")
    if arc_lines != problem[1]:
        raise ValueError(
            f"{path}: problem line declares {problem[1]} arcs but file contains {arc_lines}"
        )
    return problem


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def resolve_dataset_path(repo: Path, generated_dir: Path | None, dataset: dict) -> Path:
    if dataset["kind"] == "generated" and generated_dir is not None:
        return generated_dir / f"{dataset['id']}.gr"
    return repo / dataset["local_path"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--registry", type=Path, default=Path("benchmarks/datasets/registry.json")
    )
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    parser.add_argument("--generated-dir", type=Path)
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args()

    repo = args.repo_root.resolve()
    registry = json.loads(args.registry.read_text(encoding="utf-8"))
    require(registry.get("schema") == "ades-dataset-registry-v1", "bad registry schema")
    require(registry.get("schema_version") == 1, "bad registry schema version")
    datasets = registry.get("datasets")
    require(isinstance(datasets, list) and datasets, "registry datasets must be non-empty")

    ids: set[str] = set()
    paths: set[str] = set()
    topology_classes: set[str] = set()
    verified: list[dict] = []

    for dataset in datasets:
        for key in (
            "id",
            "display_name",
            "kind",
            "topology_class",
            "weight_semantics",
            "format",
            "directed_representation",
            "vertices",
            "arcs",
            "local_path",
            "sha256",
            "license",
            "qualification",
        ):
            require(key in dataset, f"dataset missing required field: {key}")

        dataset_id = dataset["id"]
        require(dataset_id not in ids, f"duplicate dataset id: {dataset_id}")
        ids.add(dataset_id)
        require(dataset["local_path"] not in paths, f"duplicate local_path: {dataset['local_path']}")
        paths.add(dataset["local_path"])
        require(dataset["kind"] in {"generated", "external-committed"},
                f"{dataset_id}: unsupported kind")
        require(isinstance(dataset["vertices"], int) and dataset["vertices"] > 0,
                f"{dataset_id}: invalid vertex count")
        require(isinstance(dataset["arcs"], int) and dataset["arcs"] > 0,
                f"{dataset_id}: invalid arc count")
        require(SHA256_RE.fullmatch(dataset["sha256"]) is not None,
                f"{dataset_id}: invalid SHA-256")

        license_info = dataset["license"]
        require(isinstance(license_info.get("status"), str) and license_info["status"].strip(),
                f"{dataset_id}: missing license status")
        require(isinstance(license_info.get("redistribution_review_required"), bool),
                f"{dataset_id}: missing redistribution-review flag")
        require(isinstance(license_info.get("external_third_party_data"), bool),
                f"{dataset_id}: missing external-data flag")

        qualification = dataset["qualification"]
        require(qualification.get("scientific_use") == "qualified",
                f"{dataset_id}: dataset is not scientifically qualified")
        require(qualification.get("hash_verified") is True,
                f"{dataset_id}: hash must be qualified")
        require(qualification.get("parser_verified") is True,
                f"{dataset_id}: parser compatibility must be qualified")
        topology_classes.add(dataset["topology_class"])

        if dataset["kind"] == "generated":
            generator = dataset.get("generator")
            require(isinstance(generator, dict), f"{dataset_id}: generated dataset lacks generator")
            require(generator.get("tool") == "tools/generate_synthetic_graphs.py",
                    f"{dataset_id}: unexpected generator tool")
            require(generator.get("generator_version") == 1,
                    f"{dataset_id}: unexpected generator version")
            require(license_info["external_third_party_data"] is False,
                    f"{dataset_id}: generated graph marked as third-party")
        else:
            source = dataset.get("source")
            require(isinstance(source, dict), f"{dataset_id}: external dataset lacks source")
            require(all(source.get(k) for k in ("collection", "upstream", "url")),
                    f"{dataset_id}: incomplete source provenance")
            require(license_info["external_third_party_data"] is True,
                    f"{dataset_id}: external graph not marked third-party")

        path = resolve_dataset_path(repo, args.generated_dir, dataset)
        require(path.is_file(), f"{dataset_id}: dataset file missing: {path}")
        actual_sha = sha256_file(path)
        require(actual_sha == dataset["sha256"],
                f"{dataset_id}: SHA mismatch: {actual_sha} != {dataset['sha256']}")
        vertices, arcs = dimacs_counts(path)
        require(vertices == dataset["vertices"],
                f"{dataset_id}: vertex-count mismatch: {vertices} != {dataset['vertices']}")
        require(arcs == dataset["arcs"],
                f"{dataset_id}: arc-count mismatch: {arcs} != {dataset['arcs']}")
        verified.append(
            {
                "id": dataset_id,
                "topology_class": dataset["topology_class"],
                "sha256": actual_sha,
                "vertices": vertices,
                "arcs": arcs,
            }
        )

    rule = registry.get("generality_rule", {})
    minimum = rule.get("minimum_distinct_topology_classes")
    require(isinstance(minimum, int) and minimum >= 5,
            "generality rule must require at least five topology classes")
    require(len(topology_classes) >= minimum,
            f"registry has {len(topology_classes)} topology classes, requires {minimum}")
    if rule.get("require_non_road_topology"):
        require(any(name != "road" for name in topology_classes),
                "registry requires at least one non-road topology")
    require(rule.get("metric_variants_do_not_count_as_distinct_topologies") is True,
            "registry must forbid metric variants from inflating topology diversity")

    road_entries = [d for d in datasets if d["topology_class"] == "road"]
    if len(road_entries) > 1:
        require(len({d["topology_class"] for d in road_entries}) == 1,
                "road metric variants must share one topology class")

    summary = {
        "schema": "ades-pr47-dataset-registry-validation-v1",
        "status": "PASS",
        "dataset_count": len(datasets),
        "distinct_topology_classes": sorted(topology_classes),
        "distinct_topology_class_count": len(topology_classes),
        "non_road_topology_class_count": sum(name != "road" for name in topology_classes),
        "verified": verified,
    }
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
