#!/usr/bin/env python3
"""PR49 publication viability pilot.

Runs matched Workload Generator v2 traces through isolated B2L/B3L and PR45
policy profiles, preserves raw evidence, computes paired seed-level confidence
intervals, and emits a predeclared GO / REFRAME / NO-GO decision.
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

PROFILES = (
    "B2L",
    "B3L",
    "COLD",
    "FREQ-LRU-REBUILD",
    "FREQ-LRU-REPAIR",
    "WORK-LRU-REPAIR",
    "WORK-DEBT-REPAIR",
    "ADES",
)
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
    "graph_id", "graph_sha256", "family", "update_interval", "seed", "repeat",
    "execution_order", *PROFILE_FIELDS, "peak_rss_kb", "wall_s", "exit_code",
)
T_CRITICAL_95_DF2 = 4.302652729911275


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_checked(cmd, *, env=None, stdout_path=None, stderr_path=None, timeout=1800):
    out = open(stdout_path, "w") if stdout_path else subprocess.PIPE
    err = open(stderr_path, "w") if stderr_path else subprocess.PIPE
    try:
        proc = subprocess.run(cmd, text=True, stdout=out, stderr=err, env=env,
                              timeout=timeout, check=False)
    finally:
        if stdout_path:
            out.close()
        if stderr_path:
            err.close()
    if proc.returncode:
        detail = ""
        if stderr_path and Path(stderr_path).is_file():
            detail = Path(stderr_path).read_text(errors="replace")[-4000:]
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(map(str, cmd))}\n{detail}")
    return proc


def parse_profile_line(line: str) -> dict[str, str]:
    parts = line.strip().split(",")
    if len(parts) != len(PROFILE_FIELDS):
        raise ValueError(f"pilot profile row has {len(parts)} fields; expected {len(PROFILE_FIELDS)}")
    return dict(zip(PROFILE_FIELDS, parts))


def geometric_mean(values):
    if not values or any(v <= 0 for v in values):
        return float("nan")
    return math.exp(sum(math.log(v) for v in values) / len(values))


def median_int(values):
    return int(statistics.median(int(v) for v in values))


def load_direct_comparators(path: Path) -> list[str]:
    data = json.loads(path.read_text())
    return [item["id"] for item in data["comparators"] if item["status"] == "DIRECT"]


def write_summary(rows: list[dict[str, str]], path: Path) -> list[dict[str, object]]:
    grouped = {}
    for row in rows:
        key = (row["graph_id"], row["family"], int(row["update_interval"]),
               int(row["budget_bytes"]), row["profile"])
        grouped.setdefault(key, []).append(row)
    output = []
    for key, group in sorted(grouped.items()):
        graph, family, interval, budget, profile = key
        output.append({
            "graph_id": graph,
            "family": family,
            "update_interval": interval,
            "budget_bytes": budget,
            "profile": profile,
            "samples": len(group),
            "median_algorithm_ns": median_int(r["algorithm_ns"] for r in group),
            "median_query_ns": median_int(r["query_ns"] for r in group),
            "median_update_ns": median_int(r["update_ns"] for r in group),
            "median_peak_rss_kb": median_int(r["peak_rss_kb"] for r in group),
            "median_peak_accounted_bytes": median_int(r["peak_accounted_bytes"] for r in group),
            "median_p95_ns": median_int(r["operation_p95_ns"] for r in group),
        })
    fields = list(output[0]) if output else []
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(output)
    return output


def write_confidence(rows: list[dict[str, str]], path: Path) -> list[dict[str, object]]:
    by_cell = {}
    for row in rows:
        cell = (row["graph_id"], row["family"], int(row["update_interval"]),
                int(row["budget_bytes"]), int(row["seed"]), int(row["repeat"]))
        by_cell.setdefault(cell, {})[row["profile"]] = row

    regime_pairs = {}
    for cell, profiles in by_cell.items():
        if "ADES" not in profiles:
            continue
        graph, family, interval, budget, seed, repeat = cell
        ades = int(profiles["ADES"]["algorithm_ns"])
        for comparator, row in profiles.items():
            if comparator == "ADES":
                continue
            speedup = int(row["algorithm_ns"]) / ades
            regime = (graph, family, interval, budget, comparator)
            regime_pairs.setdefault(regime, []).append((seed, repeat, speedup))

    output = []
    for key, pairs in sorted(regime_pairs.items()):
        graph, family, interval, budget, comparator = key
        seed_logs = {}
        wins = ties = losses = 0
        for seed, repeat, speedup in pairs:
            seed_logs.setdefault(seed, []).append(math.log(speedup))
            if speedup > 1.01:
                wins += 1
            elif speedup < 1 / 1.01:
                losses += 1
            else:
                ties += 1
        seed_means = [statistics.mean(v) for _, v in sorted(seed_logs.items())]
        mean_log = statistics.mean(seed_means)
        geomean = math.exp(mean_log)
        if len(seed_means) >= 2:
            sd = statistics.stdev(seed_means)
            tcrit = T_CRITICAL_95_DF2 if len(seed_means) == 3 else 1.96
            half = tcrit * sd / math.sqrt(len(seed_means))
            lo, hi = math.exp(mean_log - half), math.exp(mean_log + half)
        else:
            lo = hi = float("nan")
        output.append({
            "graph_id": graph,
            "family": family,
            "update_interval": interval,
            "budget_bytes": budget,
            "comparator": comparator,
            "paired_samples": len(pairs),
            "independent_seeds": len(seed_means),
            "geomean_comparator_over_ades": f"{geomean:.9f}",
            "ci95_low": "NA" if math.isnan(lo) else f"{lo:.9f}",
            "ci95_high": "NA" if math.isnan(hi) else f"{hi:.9f}",
            "ades_wins": wins,
            "ties": ties,
            "ades_losses": losses,
        })
    fields = list(output[0]) if output else []
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(output)
    return output


def decide(summary, confidence, direct_external: list[str]) -> dict[str, object]:
    regimes = {}
    for row in summary:
        key = (row["graph_id"], row["family"], row["update_interval"], row["budget_bytes"])
        regimes.setdefault(key, {})[row["profile"]] = row
    fastest = {}
    for key, profiles in regimes.items():
        winner = min(profiles, key=lambda p: int(profiles[p]["median_algorithm_ns"]))
        fastest["|".join(map(str, key))] = winner
    unique_fastest = sorted(set(fastest.values()))
    crossover = len(unique_fastest) > 1
    ades_fastest_regimes = sum(1 for p in fastest.values() if p == "ADES")

    simple = [r for r in confidence if r["comparator"] == "FREQ-LRU-REPAIR"]
    material_simple_wins = []
    material_simple_losses = []
    for row in simple:
        gm = float(row["geomean_comparator_over_ades"])
        lo = None if row["ci95_low"] == "NA" else float(row["ci95_low"])
        hi = None if row["ci95_high"] == "NA" else float(row["ci95_high"])
        if gm >= 1.10 and lo is not None and lo > 1.0:
            material_simple_wins.append(row)
        if gm <= 1 / 1.10 and hi is not None and hi < 1.0:
            material_simple_losses.append(row)

    headline_sota_ready = bool(direct_external)
    internal_signal = bool(material_simple_wins) or crossover or ades_fastest_regimes > 0
    if headline_sota_ready and material_simple_wins:
        decision = "GO"
        reason = "At least one DIRECT external comparator is qualified and the pilot shows material controlled ADES wins."
    elif internal_signal:
        decision = "REFRAME"
        reason = (
            "The pilot shows a nontrivial operating-region/crossover signal, but the scoped SOTA headline remains blocked "
            "until a DIRECT external comparator is qualified."
        )
    else:
        decision = "NO-GO"
        reason = "The pilot does not show a material ADES advantage or meaningful crossover against simpler controls."
    return {
        "decision": decision,
        "reason": reason,
        "headline_sota_ready": headline_sota_ready,
        "direct_external_comparators": direct_external,
        "regime_count": len(regimes),
        "unique_fastest_profiles": unique_fastest,
        "crossover_observed": crossover,
        "ades_fastest_regimes": ades_fastest_regimes,
        "material_ades_wins_vs_freq_lru_repair": len(material_simple_wins),
        "material_ades_losses_vs_freq_lru_repair": len(material_simple_losses),
    }


def write_analysis(path: Path, decision, summary, confidence):
    fastest_counts = {}
    regimes = {}
    for row in summary:
        key = (row["graph_id"], row["family"], row["update_interval"], row["budget_bytes"])
        regimes.setdefault(key, {})[row["profile"]] = row
    for profiles in regimes.values():
        winner = min(profiles, key=lambda p: int(profiles[p]["median_algorithm_ns"]))
        fastest_counts[winner] = fastest_counts.get(winner, 0) + 1
    simple = [r for r in confidence if r["comparator"] == "FREQ-LRU-REPAIR"]
    simple_sorted = sorted(simple, key=lambda r: float(r["geomean_comparator_over_ades"]), reverse=True)

    lines = [
        "# PR49 Publication Viability Pilot — Analysis",
        "",
        f"## Decision: {decision['decision']}",
        "",
        decision["reason"],
        "",
        "This is a pilot viability decision, not the definitive manuscript conclusion.",
        "",
        "## Frozen decision facts",
        "",
        f"- DIRECT external comparators: {len(decision['direct_external_comparators'])}",
        f"- Headline scoped-SOTA gate open: {str(decision['headline_sota_ready']).lower()}",
        f"- Tested runtime-memory regimes: {decision['regime_count']}",
        f"- Crossover observed: {str(decision['crossover_observed']).lower()}",
        f"- ADES fastest regimes: {decision['ades_fastest_regimes']}",
        f"- Material ADES wins vs FREQ-LRU-REPAIR: {decision['material_ades_wins_vs_freq_lru_repair']}",
        f"- Material ADES losses vs FREQ-LRU-REPAIR: {decision['material_ades_losses_vs_freq_lru_repair']}",
        "",
        "## Fastest-profile counts",
        "",
    ]
    for profile, count in sorted(fastest_counts.items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"- {profile}: {count}")
    lines += ["", "## Strongest ADES-vs-simple cells", ""]
    for row in simple_sorted[:8]:
        lines.append(
            f"- {row['graph_id']} / {row['family']} / update 1/{row['update_interval']} / "
            f"budget {row['budget_bytes']}: comparator/ADES={row['geomean_comparator_over_ades']} "
            f"(95% CI {row['ci95_low']}–{row['ci95_high']})"
        )
    lines += [
        "",
        "## Interpretation constraints",
        "",
        "- A ratio above 1.0 in confidence.csv means ADES was faster than the named comparator.",
        "- Confidence intervals are computed on seed-level mean log speedups; the two repetitions per seed measure runtime repeatability and are not treated as independent workload samples.",
        "- With only three independent seeds, the pilot CI is intentionally conservative (t critical value for df=2).",
        "- All losses and contradictory cells remain in measurements.csv and confidence.csv.",
        "- C-SOTA cannot be declared while the PR48 DIRECT external comparator set is empty.",
    ]
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
    parser.add_argument("--output", type=Path, default=Path("results/pr49-pilot"))
    parser.add_argument("--queries", type=int, default=60)
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()
    repo = args.repo.resolve()
    build = (repo / args.build).resolve() if not args.build.is_absolute() else args.build
    out = (repo / args.output).resolve() if not args.output.is_absolute() else args.output
    out.mkdir(parents=True, exist_ok=True)
    raw_dir = out / "raw"; raw_dir.mkdir(exist_ok=True)
    trace_dir = out / "traces"; trace_dir.mkdir(exist_ok=True)
    oracle_dir = out / "oracles"; oracle_dir.mkdir(exist_ok=True)
    generated = out / "generated-graphs"; generated.mkdir(exist_ok=True)

    generator = build / "ades_workload_v2"
    profile_bin = build / "ades_pilot_profile"
    if not generator.is_file() or not profile_bin.is_file():
        raise SystemExit("build/ades_workload_v2 and build/ades_pilot_profile are required")

    if args.smoke:
        graphs = [("syn-grid-224x224-v1", generated / "syn-grid-224x224-v1.gr")]
        families = [("single-hot", 95)]
        intervals = [5]
        budgets = [16 * 1024 * 1024]
        seeds = [7]
        repeats = [0]
        queries = min(args.queries, 12)
    else:
        graphs = [
            ("ny-road-distance", repo / "benchmarks/data/USA-road-d.NY.gr.gz"),
            ("syn-grid-224x224-v1", generated / "syn-grid-224x224-v1.gr"),
            ("syn-scale-free-50000-m4-v1", generated / "syn-scale-free-50000-m4-v1.gr"),
        ]
        families = [("uniform", 0), ("single-hot", 95), ("churn", 100)]
        intervals = [5, 50]
        budgets = [16 * 1024 * 1024, 64 * 1024 * 1024]
        seeds = [7, 17, 29]
        repeats = [0, 1]
        queries = args.queries

    synthetic = [gid for gid, path in graphs if gid.startswith("syn-")]
    for graph_id in synthetic:
        run_checked([sys.executable, str(repo / "tools/generate_synthetic_graphs.py"),
                     str(generated), "--only", graph_id])

    graph_hashes = {gid: sha256_file(path) for gid, path in graphs}
    rows = []
    trace_index = 0
    for graph_id, graph_path in graphs:
        for family, locality in families:
            for interval in intervals:
                for seed in seeds:
                    trace_index += 1
                    stem = f"{graph_id}__{family}__u{interval}__s{seed}"
                    trace = trace_dir / f"{stem}.trace"
                    meta = trace_dir / f"{stem}.json"
                    oracle = oracle_dir / f"{stem}.oracle"
                    run_checked([
                        str(generator), str(graph_path), str(trace), str(meta), str(seed),
                        str(queries), family, str(interval), "strict-alternating", "medium",
                        "4", "15", str(locality), "4",
                    ], stdout_path=raw_dir / f"{stem}.generator.stdout",
                       stderr_path=raw_dir / f"{stem}.generator.stderr")
                    run_checked([str(profile_bin), "oracle", str(graph_path), str(trace), str(oracle)],
                                stdout_path=raw_dir / f"{stem}.oracle.stdout",
                                stderr_path=raw_dir / f"{stem}.oracle.stderr")
                    metadata = json.loads(meta.read_text())
                    for budget_index, budget in enumerate(budgets):
                        for repeat in repeats:
                            rotation = (trace_index + budget_index + repeat) % len(PROFILES)
                            order = PROFILES[rotation:] + PROFILES[:rotation]
                            for order_index, profile in enumerate(order):
                                run_key = f"{stem}__b{budget}__r{repeat}__{profile}"
                                stdout_file = raw_dir / f"{run_key}.stdout"
                                stderr_file = raw_dir / f"{run_key}.stderr"
                                rss_file = raw_dir / f"{run_key}.rss"
                                cmd = [
                                    "/usr/bin/time", "-f", "%M", "-o", str(rss_file),
                                    str(profile_bin), "run", str(graph_path), str(trace), str(oracle),
                                    str(budget), profile,
                                ]
                                started = time.monotonic()
                                proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
                                wall = time.monotonic() - started
                                stdout_file.write_text(proc.stdout)
                                stderr_file.write_text(proc.stderr)
                                if proc.returncode:
                                    raise RuntimeError(f"pilot run failed: {run_key}\n{proc.stderr[-4000:]}")
                                parsed = parse_profile_line(proc.stdout)
                                if parsed["trace_sha256"] != metadata["trace_sha256"]:
                                    raise RuntimeError("profile trace identity mismatch")
                                if int(parsed["budget_bytes"]) != budget:
                                    raise RuntimeError("profile budget mismatch")
                                row = {
                                    "graph_id": graph_id,
                                    "graph_sha256": graph_hashes[graph_id],
                                    "family": family,
                                    "update_interval": interval,
                                    "seed": seed,
                                    "repeat": repeat,
                                    "execution_order": order_index,
                                    **parsed,
                                    "peak_rss_kb": rss_file.read_text().strip(),
                                    "wall_s": f"{wall:.6f}",
                                    "exit_code": proc.returncode,
                                }
                                rows.append(row)

    measurements = out / "measurements.csv"
    with measurements.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=MEASUREMENT_FIELDS)
        writer.writeheader(); writer.writerows(rows)

    summary = write_summary(rows, out / "summary.csv")
    confidence = write_confidence(rows, out / "confidence.csv")
    direct = load_direct_comparators(repo / "benchmarks/comparators/registry.json")
    decision = decide(summary, confidence, direct)
    (out / "decision.json").write_text(json.dumps(decision, indent=2) + "\n")
    write_analysis(out / "ANALYSIS.md", decision, summary, confidence)

    manifest = {
        "schema": "ades-pr49-publication-viability-pilot-v1",
        "claim_ids": ["C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "C-SOTA"],
        "commit": os.environ.get("GITHUB_SHA", "local-unpinned"),
        "mode": "smoke" if args.smoke else "pilot",
        "graphs": [{"id": gid, "sha256": graph_hashes[gid]} for gid, _ in graphs],
        "families": [name for name, _ in families],
        "update_intervals": intervals,
        "budgets_bytes": budgets,
        "seeds": seeds,
        "repetitions_per_seed": len(repeats),
        "queries_per_trace": queries,
        "update_mode": "strict-alternating",
        "magnitude": "medium",
        "profiles": list(PROFILES),
        "timing": "one comparator per process; oracle generated in a separate process and excluded from algorithm timing/RSS",
        "statistics": "paired comparator/ADES log speedups; repetitions averaged within seed; 95% t interval across independent seeds",
        "decision_rule": "GO requires a nonempty PR48 DIRECT external comparator set plus material controlled ADES wins; otherwise crossover/material internal signal => REFRAME; absent internal signal => NO-GO",
        "decision": decision,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_sha256s(out)
    print(json.dumps(decision, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
