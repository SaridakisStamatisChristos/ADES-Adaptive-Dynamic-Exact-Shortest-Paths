#!/usr/bin/env python3
"""Validate ADES dataset provenance, hashes, counts, and topology diversity.

Registry v2 adds ``external-qualified-remote`` entries. Those entries are not
silently treated as locally reverified files: the validator requires frozen raw
and canonical hashes plus immutable GitHub evidence identifiers from a prior
qualification PR/run/artifact. This lets the registry track externally sourced
real topologies without committing third-party bytes merely to satisfy the
registry schema.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
LOCAL_KINDS = {"generated", "external-committed"}
REMOTE_KIND = "external-qualified-remote"
SUPPORTED_KINDS = LOCAL_KINDS | {REMOTE_KIND}


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


def require_sha(value: object, message: str) -> str:
    require(isinstance(value, str) and SHA256_RE.fullmatch(value) is not None, message)
    return str(value)


def validate_evidence(dataset_id: str, qualification: dict) -> dict:
    evidence = qualification.get("evidence")
    require(isinstance(evidence, dict), f"{dataset_id}: remote dataset lacks evidence object")
    require(isinstance(evidence.get("pr"), int) and evidence["pr"] > 0,
            f"{dataset_id}: invalid evidence PR")
    require(isinstance(evidence.get("canonical_run"), int) and evidence["canonical_run"] > 0,
            f"{dataset_id}: invalid canonical workflow run")
    require(isinstance(evidence.get("canonical_artifact"), int) and evidence["canonical_artifact"] > 0,
            f"{dataset_id}: invalid canonical artifact id")
    require(isinstance(evidence.get("canonical_head"), str)
            and COMMIT_RE.fullmatch(evidence["canonical_head"]) is not None,
            f"{dataset_id}: invalid canonical head SHA")
    require_sha(evidence.get("canonical_artifact_sha256"),
                f"{dataset_id}: invalid canonical artifact SHA-256")
    return evidence


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
    schema = registry.get("schema")
    version = registry.get("schema_version")
    require(schema in {"ades-dataset-registry-v1", "ades-dataset-registry-v2"},
            "bad registry schema")
    require(version in {1, 2}, "bad registry schema version")
    require((schema.endswith("v1") and version == 1) or (schema.endswith("v2") and version == 2),
            "registry schema/version mismatch")
    datasets = registry.get("datasets")
    require(isinstance(datasets, list) and datasets, "registry datasets must be non-empty")

    ids: set[str] = set()
    paths: set[str] = set()
    topology_classes: set[str] = set()
    external_real_topology_classes: set[str] = set()
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
            "license",
            "qualification",
        ):
            require(key in dataset, f"dataset missing required field: {key}")

        dataset_id = dataset["id"]
        require(dataset_id not in ids, f"duplicate dataset id: {dataset_id}")
        ids.add(dataset_id)
        kind = dataset["kind"]
        require(kind in SUPPORTED_KINDS, f"{dataset_id}: unsupported kind")
        if kind == REMOTE_KIND:
            require(version >= 2, f"{dataset_id}: remote-qualified entry requires registry v2")
        require(isinstance(dataset["vertices"], int) and dataset["vertices"] > 0,
                f"{dataset_id}: invalid vertex count")
        require(isinstance(dataset["arcs"], int) and dataset["arcs"] > 0,
                f"{dataset_id}: invalid arc count")
        require(isinstance(dataset["directed_representation"], bool),
                f"{dataset_id}: directed_representation must be boolean")

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

        if kind == "generated":
            for key in ("local_path", "sha256"):
                require(key in dataset, f"{dataset_id}: generated dataset missing {key}")
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
            external_real_topology_classes.add(dataset["topology_class"])

        if kind in LOCAL_KINDS:
            for key in ("local_path", "sha256"):
                require(key in dataset, f"{dataset_id}: local dataset missing {key}")
            local_path = dataset["local_path"]
            require(isinstance(local_path, str) and local_path,
                    f"{dataset_id}: invalid local_path")
            require(local_path not in paths, f"duplicate local_path: {local_path}")
            paths.add(local_path)
            expected_sha = require_sha(dataset["sha256"], f"{dataset_id}: invalid SHA-256")

            path = resolve_dataset_path(repo, args.generated_dir, dataset)
            require(path.is_file(), f"{dataset_id}: dataset file missing: {path}")
            actual_sha = sha256_file(path)
            require(actual_sha == expected_sha,
                    f"{dataset_id}: SHA mismatch: {actual_sha} != {expected_sha}")
            vertices, arcs = dimacs_counts(path)
            require(vertices == dataset["vertices"],
                    f"{dataset_id}: vertex-count mismatch: {vertices} != {dataset['vertices']}")
            require(arcs == dataset["arcs"],
                    f"{dataset_id}: arc-count mismatch: {arcs} != {dataset['arcs']}")
            verified.append(
                {
                    "id": dataset_id,
                    "topology_class": dataset["topology_class"],
                    "verification_mode": "local-byte-and-dimacs-validation",
                    "sha256": actual_sha,
                    "vertices": vertices,
                    "arcs": arcs,
                }
            )
        else:
            raw_sha = require_sha(dataset.get("source_artifact_sha256"),
                                  f"{dataset_id}: invalid source artifact SHA-256")
            canonical_sha = require_sha(dataset.get("canonical_topology_sha256"),
                                        f"{dataset_id}: invalid canonical topology SHA-256")
            canonicalization = dataset.get("canonicalization")
            require(isinstance(canonicalization, str) and canonicalization.strip(),
                    f"{dataset_id}: missing canonicalization rule")
            require(isinstance(dataset.get("source_data_rows"), int)
                    and dataset["source_data_rows"] >= dataset["arcs"],
                    f"{dataset_id}: invalid source_data_rows")
            require(isinstance(dataset.get("duplicate_rows"), int) and dataset["duplicate_rows"] >= 0,
                    f"{dataset_id}: invalid duplicate_rows")
            require(isinstance(dataset.get("self_loops"), int) and dataset["self_loops"] >= 0,
                    f"{dataset_id}: invalid self_loops")
            require(dataset["source_data_rows"] - dataset["duplicate_rows"] == dataset["arcs"],
                    f"{dataset_id}: source row/duplicate/arc accounting mismatch")
            evidence = validate_evidence(dataset_id, qualification)
            require(license_info["redistribution_review_required"] is True,
                    f"{dataset_id}: remote third-party bytes require explicit redistribution review")
            verified.append(
                {
                    "id": dataset_id,
                    "topology_class": dataset["topology_class"],
                    "verification_mode": "prior-canonical-evidence-reference",
                    "source_artifact_sha256": raw_sha,
                    "canonical_topology_sha256": canonical_sha,
                    "vertices": dataset["vertices"],
                    "arcs": dataset["arcs"],
                    "evidence_pr": evidence["pr"],
                    "canonical_head": evidence["canonical_head"],
                    "canonical_run": evidence["canonical_run"],
                    "canonical_artifact": evidence["canonical_artifact"],
                    "canonical_artifact_sha256": evidence["canonical_artifact_sha256"],
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

    external_minimum = rule.get("minimum_external_real_topology_classes", 1)
    require(isinstance(external_minimum, int) and external_minimum >= 1,
            "external-real topology minimum must be a positive integer")
    require(len(external_real_topology_classes) >= external_minimum,
            f"registry has {len(external_real_topology_classes)} external real topology classes, "
            f"requires {external_minimum}")

    road_entries = [d for d in datasets if d["topology_class"] == "road"]
    if len(road_entries) > 1:
        require(len({d["topology_class"] for d in road_entries}) == 1,
                "road metric variants must share one topology class")

    summary = {
        "schema": "ades-dataset-registry-validation-v2" if version == 2
                  else "ades-pr47-dataset-registry-validation-v1",
        "status": "PASS",
        "dataset_count": len(datasets),
        "distinct_topology_classes": sorted(topology_classes),
        "distinct_topology_class_count": len(topology_classes),
        "non_road_topology_class_count": sum(name != "road" for name in topology_classes),
        "external_real_topology_classes": sorted(external_real_topology_classes),
        "external_real_topology_class_count": len(external_real_topology_classes),
        "verified": verified,
    }
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
