#!/usr/bin/env python3
"""PR51 ADES-v3 regression + second-holdout benchmark.

Regression deliberately reuses consumed PR49/PR50 failure cells and is tuning evidence only.
Holdout2 freezes new seeds, budgets, update directions/magnitudes and graph/workload
pairings before observing ADES-v3 performance.
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

PROFILES = ("COLD", "B3L", "FREQ-LRU-REPAIR", "ADES", "ADES-V2", "ADES-V3")
COMPARATORS = ("COLD", "B3L", "FREQ-LRU-REPAIR", "ADES", "ADES-V2")
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
    "phase", "graph_id", "graph_sha256", "family", "locality_percent",
    "update_interval", "update_mode", "magnitude", "seed", "repeat",
    "execution_order", *PROFILE_FIELDS, "peak_rss_kb", "wall_s",
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


def regression_cases():
    mib = 1024 * 1024
    cases = []
    # Consumed PR49/PR50 churn failures: development evidence only.
    for graph_id, graph_file in (
        ("ny-road-distance", "benchmarks/data/USA-road-d.NY.gr.gz"),
        ("syn-grid-224x224-v1", "syn-grid-224x224-v1.gr"),
    ):
        for interval in (5, 50):
            cases.append(dict(graph_id=graph_id, graph_file=graph_file, family="churn",
                              locality=100, interval=interval, mode="strict-alternating",
                              magnitude="medium", budgets=(16*mib, 64*mib)))
    # The one statistically material PR50 holdout contradiction.
    cases.append(dict(graph_id="syn-clustered-50000-c100-d6-v1",
                      graph_file="syn-clustered-50000-c100-d6-v1.gr",
                      family="hot-pool", locality=85, interval=2,
                      mode="repeated-edge", magnitude="medium", budgets=(16*mib,)))
    return cases


def holdout2_cases():
    mib = 1024 * 1024
    cases = []
    graphs = (
        ("ny-road-distance", "benchmarks/data/USA-road-d.NY.gr.gz"),
        ("syn-scale-free-50000-m4-v1", "syn-scale-free-50000-m4-v1.gr"),
        ("syn-small-world-50000-k8-r10-v1", "syn-small-world-50000-k8-r10-v1.gr"),
    )
    families = (("hot-pool", 70), ("rotating-hot", 80))
    scenarios = (("balanced-random", "small"), ("increase-only", "large"))
    for graph_id, graph_file in graphs:
        for family, locality in families:
            for mode, magnitude in scenarios:
                for interval in (10, 100):
                    cases.append(dict(graph_id=graph_id, graph_file=graph_file,
                                      family=family, locality=locality, interval=interval,
                                      mode=mode, magnitude=magnitude,
                                      budgets=(24*mib, 48*mib)))
    return cases


def matrix(smoke: bool, phase: str):
    if smoke:
        return {
            "cases": [dict(graph_id="syn-grid-224x224-v1", graph_file="syn-grid-224x224-v1.gr",
                           family="single-hot", locality=95, interval=5,
                           mode="strict-alternating", magnitude="medium",
                           budgets=(16*1024*1024,))],
            "seeds": (7,), "repeats": (0,),
        }
    if phase == "regression":
        return {"cases": regression_cases(), "seeds": (7, 17, 29), "repeats": (0,)}
    return {"cases": holdout2_cases(), "seeds": (101, 137, 181), "repeats": (0,)}


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

    graph_paths = {}
    for case in spec["cases"]:
        gid = case["graph_id"]
        if gid not in graph_paths:
            graph_paths[gid] = ensure_graph(repo, generated, gid, case["graph_file"])
    graph_hashes = {gid: sha256_file(path) for gid, path in graph_paths.items()}

    rows: list[dict[str, object]] = []
    trace_index = 0
    seen_traces = set()
    for case_index, case in enumerate(spec["cases"]):
        graph_id = case["graph_id"]
        graph_path = graph_paths[graph_id]
        for seed in spec["seeds"]:
            trace_index += 1
            stem = (f"{phase}__{case_index:03d}__{graph_id}__{case['family']}__"
                    f"l{case['locality']}__u{case['interval']}__{case['mode']}__"
                    f"{case['magnitude']}__s{seed}")
            trace = traces / f"{stem}.trace"
            meta = traces / f"{stem}.json"
            oracle = oracles / f"{stem}.oracle"
            run_checked([
                str(generator), str(graph_path), str(trace), str(meta), str(seed),
                str(queries), case["family"], str(case["interval"]), case["mode"],
                case["magnitude"], "4", "15", str(case["locality"]), "4",
            ], stdout_path=raw / f"{stem}.generator.stdout",
               stderr_path=raw / f"{stem}.generator.stderr")
            run_checked([str(runner), "oracle", str(graph_path), str(trace), str(oracle)],
                        stdout_path=raw / f"{stem}.oracle.stdout",
                        stderr_path=raw / f"{stem}.oracle.stderr")
            metadata = json.loads(meta.read_text())
            if metadata["trace_sha256"] in seen_traces:
                raise RuntimeError("unexpected duplicate trace SHA in PR51 matrix")
            seen_traces.add(metadata["trace_sha256"])
            for budget_index, budget in enumerate(case["budgets"]):
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
                            "phase": phase, "graph_id": graph_id,
                            "graph_sha256": graph_hashes[graph_id],
                            "family": case["family"], "locality_percent": case["locality"],
                            "update_interval": case["interval"], "update_mode": case["mode"],
                            "magnitude": case["magnitude"], "seed": seed, "repeat": repeat,
                            "execution_order": order_index, **parsed,
                            "peak_rss_kb": rss.read_text().strip(), "wall_s": f"{wall:.6f}",
                        })
    return rows, spec, graph_hashes


def write_measurements(rows, path: Path):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=MEASUREMENT_FIELDS)
        writer.writeheader(); writer.writerows(rows)


def summarize(rows):
    grouped = {}
    for row in rows:
        key = (row["phase"], row["graph_id"], row["family"], row["locality_percent"],
               row["update_interval"], row["update_mode"], row["magnitude"],
               row["budget_bytes"], row["profile"])
        grouped.setdefault(key, []).append(row)
    output = []
    for key, group in sorted(grouped.items()):
        phase, graph, family, locality, interval, mode, magnitude, budget, profile = key
        output.append({
            "phase": phase, "graph_id": graph, "family": family,
            "locality_percent": locality, "update_interval": interval,
            "update_mode": mode, "magnitude": magnitude, "budget_bytes": budget,
            "profile": profile, "samples": len(group),
            "median_algorithm_ns": median_int(r["algorithm_ns"] for r in group),
            "median_query_ns": median_int(r["query_ns"] for r in group),
            "median_update_ns": median_int(r["update_ns"] for r in group),
            "median_peak_rss_kb": median_int(r["peak_rss_kb"] for r in group),
            "median_peak_accounted_bytes": median_int(r["peak_accounted_bytes"] for r in group),
            "median_cold_queries": median_int(r["cold_queries"] for r in group),
            "median_promotions": median_int(r["promotions"] for r in group),
        })
    return output


def confidence(rows):
    by_cell = {}
    for row in rows:
        cell = (row["phase"], row["graph_id"], row["family"], int(row["locality_percent"]),
                int(row["update_interval"]), row["update_mode"], row["magnitude"],
                int(row["budget_bytes"]), int(row["seed"]), int(row["repeat"]))
        by_cell.setdefault(cell, {})[row["profile"]] = row
    pairs = {}
    for cell, profiles in by_cell.items():
        if "ADES-V3" not in profiles:
            continue
        phase, graph, family, locality, interval, mode, magnitude, budget, seed, repeat = cell
        v3 = int(profiles["ADES-V3"]["algorithm_ns"])
        for comparator in COMPARATORS:
            if comparator not in profiles:
                continue
            speedup = int(profiles[comparator]["algorithm_ns"]) / v3
            key = (phase, graph, family, locality, interval, mode, magnitude, budget, comparator)
            pairs.setdefault(key, []).append((seed, repeat, speedup))
    output = []
    for key, values in sorted(pairs.items()):
        phase, graph, family, locality, interval, mode, magnitude, budget, comparator = key
        seed_logs = {}
        wins = ties = losses = 0
        for seed, repeat, speedup in values:
            seed_logs.setdefault(seed, []).append(math.log(speedup))
            if speedup > 1.01: wins += 1
            elif speedup < 1 / 1.01: losses += 1
            else: ties += 1
        means = [statistics.mean(v) for _, v in sorted(seed_logs.items())]
        mean = statistics.mean(means); gm = math.exp(mean)
        if len(means) >= 2:
            sd = statistics.stdev(means)
            tcrit = T95_DF2 if len(means) == 3 else 1.96
            half = tcrit * sd / math.sqrt(len(means))
            lo, hi = math.exp(mean - half), math.exp(mean + half)
        else:
            lo = hi = float("nan")
        output.append({
            "phase": phase, "graph_id": graph, "family": family,
            "locality_percent": locality, "update_interval": interval,
            "update_mode": mode, "magnitude": magnitude, "budget_bytes": budget,
            "comparator": comparator, "paired_samples": len(values),
            "independent_seeds": len(means),
            "geomean_comparator_over_v3": f"{gm:.9f}",
            "ci95_low": "NA" if math.isnan(lo) else f"{lo:.9f}",
            "ci95_high": "NA" if math.isnan(hi) else f"{hi:.9f}",
            "v3_wins": wins, "ties": ties, "v3_losses": losses,
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
        key = (row["phase"], row["graph_id"], row["family"], row["locality_percent"],
               row["update_interval"], row["update_mode"], row["magnitude"], row["budget_bytes"])
        regimes.setdefault(key, {})[row["profile"]] = int(row["median_algorithm_ns"])
    phase_counts = {}
    fastest_counts = {}
    near_fastest = {}
    for key, profiles in regimes.items():
        phase = key[0]; fastest = min(profiles.values()); winner = min(profiles, key=profiles.get)
        phase_counts[phase] = phase_counts.get(phase, 0) + 1
        fastest_counts[(phase, winner)] = fastest_counts.get((phase, winner), 0) + 1
        if profiles.get("ADES-V3", 2**63) <= int(math.ceil(fastest * 1.01)):
            near_fastest[phase] = near_fastest.get(phase, 0) + 1

    regressions = {"regression": [], "holdout2": []}
    wins = {"regression": [], "holdout2": []}
    for row in confidence_rows:
        phase = row["phase"]
        gm = float(row["geomean_comparator_over_v3"])
        lo = None if row["ci95_low"] == "NA" else float(row["ci95_low"])
        hi = None if row["ci95_high"] == "NA" else float(row["ci95_high"])
        if gm <= 1 / 1.10 and hi is not None and hi < 1.0:
            regressions.setdefault(phase, []).append(row)
        if gm >= 1.10 and lo is not None and lo > 1.0:
            wins.setdefault(phase, []).append(row)

    return {
        "regression_regimes": phase_counts.get("regression", 0),
        "holdout2_regimes": phase_counts.get("holdout2", 0),
        "regression_v3_fastest_or_within_1pct": near_fastest.get("regression", 0),
        "holdout2_v3_fastest_or_within_1pct": near_fastest.get("holdout2", 0),
        "regression_material_regressions": len(regressions.get("regression", [])),
        "holdout2_material_regressions": len(regressions.get("holdout2", [])),
        "regression_material_wins": len(wins.get("regression", [])),
        "holdout2_material_wins": len(wins.get("holdout2", [])),
        "holdout2_zero_material_regressions": len(regressions.get("holdout2", [])) == 0,
        "fastest_profile_counts": {
            f"{phase}:{profile}": count for (phase, profile), count in sorted(fastest_counts.items())
        },
    }


def write_analysis(path: Path, result, confidence_rows):
    lines = [
        "# PR51 ADES-v3 — Regression + Second Holdout Analysis", "",
        "## Evidence classification", "",
        "- `regression` reuses previously observed failure cells and is tuning/regression evidence only.",
        "- `holdout2` was frozen before observing ADES-v3 performance and uses new seeds, budgets, update modes/magnitudes and graph/workload pairings.",
        "- Performance losses are preserved; exactness, identity and byte-budget failures are hard failures.", "",
        "## Result summary", "",
    ]
    for key, value in result.items():
        if key != "fastest_profile_counts": lines.append(f"- {key}: {value}")
    lines += ["", "## Fastest-profile counts", ""]
    for key, value in result["fastest_profile_counts"].items(): lines.append(f"- {key}: {value}")
    lines += ["", "## Largest second-holdout regressions vs ADES-V3", ""]
    holdout = [r for r in confidence_rows if r["phase"] == "holdout2"]
    for row in sorted(holdout, key=lambda r: float(r["geomean_comparator_over_v3"]))[:16]:
        lines.append(
            f"- {row['graph_id']} / {row['family']}@{row['locality_percent']} / "
            f"{row['update_mode']}:{row['magnitude']} / 1/{row['update_interval']} / "
            f"{row['budget_bytes']} bytes / {row['comparator']}: comparator/v3="
            f"{row['geomean_comparator_over_v3']} (95% CI {row['ci95_low']}–{row['ci95_high']})"
        )
    path.write_text("\n".join(lines) + "\n")


def write_sha256s(root: Path):
    files = sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS")
    (root / "SHA256SUMS").write_text(
        "".join(f"{sha256_file(p)}  {p.relative_to(root).as_posix()}\n" for p in files)
    )


def serializable_spec(spec):
    return {
        "cases": [{**c, "budgets": list(c["budgets"])} for c in spec["cases"]],
        "seeds": list(spec["seeds"]), "repeats": list(spec["repeats"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path("."))
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path, default=Path("results/pr51-fast"))
    parser.add_argument("--queries", type=int, default=60)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--phase", choices=("regression", "holdout2", "both"), default="both")
    args = parser.parse_args()
    repo = args.repo.resolve()
    build = (repo / args.build).resolve() if not args.build.is_absolute() else args.build
    out = (repo / args.output).resolve() if not args.output.is_absolute() else args.output
    out.mkdir(parents=True, exist_ok=True)

    phases = ["regression"] if args.smoke else ([args.phase] if args.phase != "both" else ["regression", "holdout2"])
    rows = []; specs = {}; hashes = {}
    for phase in phases:
        phase_rows, spec, graph_hashes = run_phase(
            repo, build, out / phase, phase,
            min(args.queries, 12) if args.smoke else args.queries, args.smoke)
        rows.extend(phase_rows); specs[phase] = serializable_spec(spec); hashes[phase] = graph_hashes

    write_measurements(rows, out / "measurements.csv")
    summary_rows = summarize(rows); write_csv(summary_rows, out / "summary.csv")
    confidence_rows = confidence(rows); write_csv(confidence_rows, out / "confidence.csv")
    result = evaluate(summary_rows, confidence_rows)
    (out / "decision.json").write_text(json.dumps(result, indent=2) + "\n")
    write_analysis(out / "ANALYSIS.md", result, confidence_rows)
    manifest = {
        "schema": "ades-pr51-fast-economic-v1",
        "commit": os.environ.get("GITHUB_SHA", "local-unpinned"),
        "profiles": list(PROFILES),
        "regression_is_tuning_evidence": True,
        "holdout2_is_fresh_evidence": not args.smoke and "holdout2" in phases,
        "queries_per_trace": min(args.queries, 12) if args.smoke else args.queries,
        "matrices": specs, "graph_sha256": hashes,
        "statistics": "paired comparator/ADES-V3 log speedups; 95% t interval across 3 independent seeds",
        "material_threshold": "10% point estimate plus CI entirely on comparator-winning/losing side of 1.0",
        "performance_gate": "informational; exactness, trace/oracle identity and byte-budget failures are hard failures",
        "result": result,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_sha256s(out)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
