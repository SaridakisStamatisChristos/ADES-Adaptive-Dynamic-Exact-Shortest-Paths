#!/usr/bin/env python3
"""PR50 ADES-v2 development + holdout benchmark.

Development deliberately reuses the PR49 matrix and is tuning/regression evidence only.
Holdout uses previously unseen graph classes, locality families, seeds, and update
modes so the controller is not judged solely on the matrix that motivated it.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys
import time

PROFILES = ("COLD", "B3L", "FREQ-LRU-REPAIR", "ADES", "ADES-V2")
COMPARATORS = ("COLD", "B3L", "FREQ-LRU-REPAIR", "ADES")
PROFILE_FIELDS = (
    "profile", "trace_sha256", "queries", "updates", "increase_count",
    "decrease_count", "budget_bytes", "algorithm_ns", "query_ns", "update_ns",
    "operation_p50_ns", "operation_p95_ns", "query_p50_ns", "query_p95_ns",
    "update_p50_ns", "update_p95_ns", "accounted_bytes", "peak_accounted_bytes",
    "cold_queries", "resident_queries", "promotions", "evictions", "rebuilds",
    "repair_aborts", "cooldown_blocks", "admission_rejections", "filtered_updates",
    "decrease_repairs", "increase_repairs", "memory_budget_rejections",
    "memory_metadata_prunes",
)
MEASUREMENT_FIELDS = (
    "phase", "graph_id", "graph_sha256", "family", "update_interval", "update_mode",
    "seed", "repeat", "execution_order", *PROFILE_FIELDS, "peak_rss_kb", "wall_s",
)
T95_DF2 = 4.302652729911275


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_checked(cmd, *, stdout_path=None, stderr_path=None, timeout=1800):
    proc = subprocess.run(cmd, text=True, capture_output=True, timeout=timeout, check=False)
    if stdout_path:
        Path(stdout_path).write_text(proc.stdout)
    if stderr_path:
        Path(stderr_path).write_text(proc.stderr)
    if proc.returncode:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(map(str, cmd))}\n{proc.stderr[-4000:]}")
    return proc


def parse_profile(line: str) -> dict[str, str]:
    parts = line.strip().split(",")
    if len(parts) != len(PROFILE_FIELDS):
        raise ValueError(f"profile row has {len(parts)} fields; expected {len(PROFILE_FIELDS)}")
    return dict(zip(PROFILE_FIELDS, parts))


def median_int(values) -> int:
    return int(statistics.median(int(v) for v in values))


def matrix(smoke: bool, phase: str):
    if smoke:
        return {
            "graphs": [("syn-grid-224x224-v1", "syn-grid-224x224-v1.gr")],
            "families": [("single-hot", 95)],
            "intervals": [5],
            "modes": ["strict-alternating"],
            "budgets": [16 * 1024 * 1024],
            "seeds": [7],
            "repeats": [0],
        }
    if phase == "development":
        return {
            "graphs": [
                ("ny-road-distance", "benchmarks/data/USA-road-d.NY.gr.gz"),
                ("syn-grid-224x224-v1", "syn-grid-224x224-v1.gr"),
                ("syn-scale-free-50000-m4-v1", "syn-scale-free-50000-m4-v1.gr"),
            ],
            "families": [("uniform", 0), ("single-hot", 95), ("churn", 100)],
            "intervals": [5, 50],
            "modes": ["strict-alternating"],
            "budgets": [16 * 1024 * 1024, 64 * 1024 * 1024],
            "seeds": [7, 17, 29],
            "repeats": [0, 1],
        }
    return {
        "graphs": [
            ("syn-uniform-50000-d6-v1", "syn-uniform-50000-d6-v1.gr"),
            ("syn-small-world-50000-k8-r10-v1", "syn-small-world-50000-k8-r10-v1.gr"),
            ("syn-clustered-50000-c100-d6-v1", "syn-clustered-50000-c100-d6-v1.gr"),
        ],
        "families": [("hot-pool", 85), ("zipf", 95), ("rotating-hot", 100)],
        "intervals": [2, 10],
        "modes": ["bursty", "repeated-edge"],
        "budgets": [16 * 1024 * 1024, 64 * 1024 * 1024],
        "seeds": [43, 59, 83],
        "repeats": [0],
    }


def ensure_graph(repo: Path, generated: Path, graph_id: str, declared: str) -> Path:
    if graph_id == "ny-road-distance":
        return repo / declared
    path = generated / declared
    if not path.is_file():
        run_checked([sys.executable, str(repo / "tools/generate_synthetic_graphs.py"),
                     str(generated), "--only", graph_id], timeout=1800)
    return path


def run_phase(repo: Path, build: Path, out: Path, phase: str, queries: int, smoke: bool):
    spec = matrix(smoke, phase)
    raw = out / "raw"; raw.mkdir(parents=True, exist_ok=True)
    traces = out / "traces"; traces.mkdir(parents=True, exist_ok=True)
    oracles = out / "oracles"; oracles.mkdir(parents=True, exist_ok=True)
    generated = out / "generated-graphs"; generated.mkdir(parents=True, exist_ok=True)
    generator = build / "ades_workload_v2"
    runner = build / "ades_pilot_profile"
    if not generator.is_file() or not runner.is_file():
        raise RuntimeError("ades_workload_v2 and ades_pilot_profile must be built")

    graph_paths = {gid: ensure_graph(repo, generated, gid, rel) for gid, rel in spec["graphs"]}
    graph_hashes = {gid: sha256_file(path) for gid, path in graph_paths.items()}
    rows: list[dict[str, object]] = []
    trace_index = 0
    for graph_id, _ in spec["graphs"]:
        graph_path = graph_paths[graph_id]
        for family, locality in spec["families"]:
            for interval in spec["intervals"]:
                for mode in spec["modes"]:
                    for seed in spec["seeds"]:
                        trace_index += 1
                        stem = f"{phase}__{graph_id}__{family}__u{interval}__{mode}__s{seed}"
                        trace = traces / f"{stem}.trace"
                        meta = traces / f"{stem}.json"
                        oracle = oracles / f"{stem}.oracle"
                        run_checked([
                            str(generator), str(graph_path), str(trace), str(meta), str(seed),
                            str(queries), family, str(interval), mode, "medium", "4", "15",
                            str(locality), "4",
                        ], stdout_path=raw / f"{stem}.generator.stdout",
                           stderr_path=raw / f"{stem}.generator.stderr")
                        run_checked([str(runner), "oracle", str(graph_path), str(trace), str(oracle)],
                                    stdout_path=raw / f"{stem}.oracle.stdout",
                                    stderr_path=raw / f"{stem}.oracle.stderr")
                        metadata = json.loads(meta.read_text())
                        for budget_index, budget in enumerate(spec["budgets"]):
                            for repeat in spec["repeats"]:
                                rotation = (trace_index + budget_index + repeat) % len(PROFILES)
                                order = PROFILES[rotation:] + PROFILES[:rotation]
                                for order_index, profile in enumerate(order):
                                    key = f"{stem}__b{budget}__r{repeat}__{profile}"
                                    rss = raw / f"{key}.rss"
                                    cmd = [
                                        "/usr/bin/time", "-f", "%M", "-o", str(rss),
                                        str(runner), "run", str(graph_path), str(trace), str(oracle),
                                        str(budget), profile,
                                    ]
                                    started = time.monotonic()
                                    proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
                                    wall = time.monotonic() - started
                                    (raw / f"{key}.stdout").write_text(proc.stdout)
                                    (raw / f"{key}.stderr").write_text(proc.stderr)
                                    if proc.returncode:
                                        raise RuntimeError(f"profile failed: {key}\n{proc.stderr[-4000:]}")
                                    parsed = parse_profile(proc.stdout)
                                    if parsed["trace_sha256"] != metadata["trace_sha256"]:
                                        raise RuntimeError("trace identity mismatch")
                                    if int(parsed["budget_bytes"]) != budget:
                                        raise RuntimeError("budget identity mismatch")
                                    if int(parsed["peak_accounted_bytes"]) > budget:
                                        raise RuntimeError("persistent-state budget violation")
                                    rows.append({
                                        "phase": phase,
                                        "graph_id": graph_id,
                                        "graph_sha256": graph_hashes[graph_id],
                                        "family": family,
                                        "update_interval": interval,
                                        "update_mode": mode,
                                        "seed": seed,
                                        "repeat": repeat,
                                        "execution_order": order_index,
                                        **parsed,
                                        "peak_rss_kb": rss.read_text().strip(),
                                        "wall_s": f"{wall:.6f}",
                                    })
    return rows, spec, graph_hashes


def write_measurements(rows, path: Path):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=MEASUREMENT_FIELDS)
        writer.writeheader(); writer.writerows(rows)


def summarize(rows):
    grouped = {}
    for row in rows:
        key = (row["phase"], row["graph_id"], row["family"], row["update_interval"],
               row["update_mode"], row["budget_bytes"], row["profile"])
        grouped.setdefault(key, []).append(row)
    output = []
    for key, group in sorted(grouped.items()):
        phase, graph, family, interval, mode, budget, profile = key
        output.append({
            "phase": phase, "graph_id": graph, "family": family,
            "update_interval": interval, "update_mode": mode, "budget_bytes": budget,
            "profile": profile, "samples": len(group),
            "median_algorithm_ns": median_int(r["algorithm_ns"] for r in group),
            "median_query_ns": median_int(r["query_ns"] for r in group),
            "median_update_ns": median_int(r["update_ns"] for r in group),
            "median_peak_rss_kb": median_int(r["peak_rss_kb"] for r in group),
            "median_peak_accounted_bytes": median_int(r["peak_accounted_bytes"] for r in group),
        })
    return output


def confidence(rows):
    by_cell = {}
    for row in rows:
        cell = (row["phase"], row["graph_id"], row["family"], int(row["update_interval"]),
                row["update_mode"], int(row["budget_bytes"]), int(row["seed"]), int(row["repeat"]))
        by_cell.setdefault(cell, {})[row["profile"]] = row
    pairs = {}
    for cell, profiles in by_cell.items():
        if "ADES-V2" not in profiles:
            continue
        phase, graph, family, interval, mode, budget, seed, repeat = cell
        v2 = int(profiles["ADES-V2"]["algorithm_ns"])
        for comparator in COMPARATORS:
            if comparator not in profiles:
                continue
            speedup = int(profiles[comparator]["algorithm_ns"]) / v2
            key = (phase, graph, family, interval, mode, budget, comparator)
            pairs.setdefault(key, []).append((seed, repeat, speedup))
    output = []
    for key, values in sorted(pairs.items()):
        phase, graph, family, interval, mode, budget, comparator = key
        seed_logs = {}
        wins = ties = losses = 0
        for seed, repeat, speedup in values:
            seed_logs.setdefault(seed, []).append(math.log(speedup))
            if speedup > 1.01: wins += 1
            elif speedup < 1 / 1.01: losses += 1
            else: ties += 1
        means = [statistics.mean(v) for _, v in sorted(seed_logs.items())]
        mean = statistics.mean(means)
        gm = math.exp(mean)
        if len(means) >= 2:
            sd = statistics.stdev(means)
            tcrit = T95_DF2 if len(means) == 3 else 1.96
            half = tcrit * sd / math.sqrt(len(means))
            lo, hi = math.exp(mean - half), math.exp(mean + half)
        else:
            lo = hi = float("nan")
        output.append({
            "phase": phase, "graph_id": graph, "family": family,
            "update_interval": interval, "update_mode": mode, "budget_bytes": budget,
            "comparator": comparator, "paired_samples": len(values),
            "independent_seeds": len(means),
            "geomean_comparator_over_v2": f"{gm:.9f}",
            "ci95_low": "NA" if math.isnan(lo) else f"{lo:.9f}",
            "ci95_high": "NA" if math.isnan(hi) else f"{hi:.9f}",
            "v2_wins": wins, "ties": ties, "v2_losses": losses,
        })
    return output


def write_csv(rows, path: Path):
    fields = list(rows[0]) if rows else []
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        if fields: writer.writeheader(); writer.writerows(rows)


def evaluate(summary_rows, confidence_rows):
    regimes = {}
    for row in summary_rows:
        key = (row["phase"], row["graph_id"], row["family"], row["update_interval"],
               row["update_mode"], row["budget_bytes"])
        regimes.setdefault(key, {})[row["profile"]] = int(row["median_algorithm_ns"])
    phase_counts = {}
    fastest_counts = {}
    near_fastest = {}
    for key, profiles in regimes.items():
        phase = key[0]
        fastest = min(profiles.values())
        winner = min(profiles, key=profiles.get)
        phase_counts[phase] = phase_counts.get(phase, 0) + 1
        fastest_counts[(phase, winner)] = fastest_counts.get((phase, winner), 0) + 1
        if "ADES-V2" in profiles and profiles["ADES-V2"] <= int(math.ceil(fastest * 1.01)):
            near_fastest[phase] = near_fastest.get(phase, 0) + 1

    material_regressions = {"development": [], "holdout": []}
    material_wins = {"development": [], "holdout": []}
    for row in confidence_rows:
        phase = row["phase"]
        gm = float(row["geomean_comparator_over_v2"])
        lo = None if row["ci95_low"] == "NA" else float(row["ci95_low"])
        hi = None if row["ci95_high"] == "NA" else float(row["ci95_high"])
        if gm <= 1 / 1.10 and hi is not None and hi < 1.0:
            material_regressions.setdefault(phase, []).append(row)
        if gm >= 1.10 and lo is not None and lo > 1.0:
            material_wins.setdefault(phase, []).append(row)

    return {
        "development_regimes": phase_counts.get("development", 0),
        "holdout_regimes": phase_counts.get("holdout", 0),
        "development_v2_fastest_or_within_1pct": near_fastest.get("development", 0),
        "holdout_v2_fastest_or_within_1pct": near_fastest.get("holdout", 0),
        "development_material_regressions": len(material_regressions.get("development", [])),
        "holdout_material_regressions": len(material_regressions.get("holdout", [])),
        "development_material_wins": len(material_wins.get("development", [])),
        "holdout_material_wins": len(material_wins.get("holdout", [])),
        "holdout_zero_material_regressions": len(material_regressions.get("holdout", [])) == 0,
        "fastest_profile_counts": {
            f"{phase}:{profile}": count for (phase, profile), count in sorted(fastest_counts.items())
        },
    }


def write_analysis(path: Path, result, confidence_rows):
    lines = [
        "# PR50 Predictive Economic Residency — Benchmark Analysis", "",
        "## Evidence classification", "",
        "- `development` reuses the PR49 matrix and is tuning/regression evidence, not independent confirmation.",
        "- `holdout` uses unseen graph classes, workload families, seeds, and update modes.",
        "- Performance losses are preserved; only exactness/budget/reproducibility failures invalidate execution.", "",
        "## Result summary", "",
    ]
    for key, value in result.items():
        if key != "fastest_profile_counts": lines.append(f"- {key}: {value}")
    lines += ["", "## Fastest-profile counts", ""]
    for key, value in result["fastest_profile_counts"].items(): lines.append(f"- {key}: {value}")
    lines += ["", "## Largest holdout regressions vs ADES-V2", ""]
    holdout = [r for r in confidence_rows if r["phase"] == "holdout"]
    for row in sorted(holdout, key=lambda r: float(r["geomean_comparator_over_v2"]))[:12]:
        lines.append(
            f"- {row['graph_id']} / {row['family']} / {row['update_mode']} / 1/{row['update_interval']} / "
            f"{row['budget_bytes']} bytes / {row['comparator']}: comparator/v2={row['geomean_comparator_over_v2']} "
            f"(95% CI {row['ci95_low']}–{row['ci95_high']})"
        )
    path.write_text("\n".join(lines) + "\n")


def write_sha256s(root: Path):
    files = sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS")
    (root / "SHA256SUMS").write_text(
        "".join(f"{sha256_file(p)}  {p.relative_to(root).as_posix()}\n" for p in files)
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path("."))
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path, default=Path("results/pr50-economic"))
    parser.add_argument("--queries", type=int, default=60)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--phase", choices=("development", "holdout", "both"), default="both")
    args = parser.parse_args()
    repo = args.repo.resolve()
    build = (repo / args.build).resolve() if not args.build.is_absolute() else args.build
    out = (repo / args.output).resolve() if not args.output.is_absolute() else args.output
    out.mkdir(parents=True, exist_ok=True)

    phases = ["development"] if args.smoke else ([args.phase] if args.phase != "both" else ["development", "holdout"])
    rows = []
    specs = {}
    hashes = {}
    for phase in phases:
        phase_rows, spec, graph_hashes = run_phase(repo, build, out / phase, phase,
                                                  min(args.queries, 12) if args.smoke else args.queries,
                                                  args.smoke)
        rows.extend(phase_rows); specs[phase] = spec; hashes[phase] = graph_hashes

    write_measurements(rows, out / "measurements.csv")
    summary_rows = summarize(rows); write_csv(summary_rows, out / "summary.csv")
    confidence_rows = confidence(rows); write_csv(confidence_rows, out / "confidence.csv")
    result = evaluate(summary_rows, confidence_rows)
    (out / "decision.json").write_text(json.dumps(result, indent=2) + "\n")
    write_analysis(out / "ANALYSIS.md", result, confidence_rows)
    manifest = {
        "schema": "ades-pr50-predictive-economic-v1",
        "commit": os.environ.get("GITHUB_SHA", "local-unpinned"),
        "profiles": list(PROFILES),
        "development_is_tuning_evidence": True,
        "holdout_is_fresh_evidence": not args.smoke and "holdout" in phases,
        "queries_per_trace": min(args.queries, 12) if args.smoke else args.queries,
        "matrices": specs,
        "graph_sha256": hashes,
        "statistics": "paired comparator/ADES-V2 log speedups; repetitions averaged within seed; 95% t interval across independent seeds",
        "performance_gate": "informational; exactness and byte-budget failures are hard execution failures",
        "result": result,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_sha256s(out)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
