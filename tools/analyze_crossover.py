#!/usr/bin/env python3
"""Produce descriptive paired summaries from crossover measurements. Stdlib only."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
import statistics

BASELINES = ("B2L", "B3L", "B4")


def median(values):
    return statistics.median(values) if values else float("nan")


def fmt(value, digits=3):
    return "NA" if value != value else f"{value:.{digits}f}"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("measurements")
    p.add_argument("--output-dir")
    args = p.parse_args()

    source = Path(args.measurements)
    out = Path(args.output_dir) if args.output_dir else source.parent
    out.mkdir(parents=True, exist_ok=True)

    with source.open(newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit("no measurement rows")

    valid = []
    for row in rows:
        if row.get("exit_code") != "0":
            continue
        try:
            row["algorithm_ns_num"] = int(row["algorithm_ns"])
            row["rss_kb_num"] = int(row["peak_child_rss_kb"])
            row["updates_num"] = int(row["updates"])
            row["increase_updates_num"] = int(row["increase_updates"])
            row["decrease_updates_num"] = int(row["decrease_updates"])
        except (KeyError, TypeError, ValueError):
            continue
        valid.append(row)

    paired = defaultdict(dict)
    for row in valid:
        paired[(row["cell"], row["repeat"])][row["baseline"]] = row

    group_pairs = defaultdict(list)
    for key, by_baseline in paired.items():
        if not all(b in by_baseline for b in BASELINES):
            continue
        exemplar = by_baseline["B4"]
        group = (
            exemplar.get("update_mode", "unknown"),
            exemplar["family"],
            int(exemplar["update_every"]),
            int(exemplar["cap"]),
        )
        group_pairs[group].append(by_baseline)

    summary_fields = [
        "update_mode", "family", "update_every", "cap", "paired_runs",
        "median_updates", "median_increase_updates", "median_decrease_updates",
        "b2l_median_algorithm_ms", "b3l_median_algorithm_ms", "b4_median_algorithm_ms",
        "b4_vs_b2l_median_speedup", "b4_vs_b3l_median_speedup",
        "b2l_median_rss_mib", "b3l_median_rss_mib", "b4_median_rss_mib",
        "b2l_vs_b4_median_rss_ratio", "b3l_vs_b4_median_rss_ratio",
    ]
    summaries = []
    for group in sorted(group_pairs):
        mode, family, ue, cap = group
        pairs = group_pairs[group]
        b2t = [x["B2L"]["algorithm_ns_num"] / 1e6 for x in pairs]
        b3t = [x["B3L"]["algorithm_ns_num"] / 1e6 for x in pairs]
        b4t = [x["B4"]["algorithm_ns_num"] / 1e6 for x in pairs]
        b2r = [x["B2L"]["rss_kb_num"] / 1024 for x in pairs]
        b3r = [x["B3L"]["rss_kb_num"] / 1024 for x in pairs]
        b4r = [x["B4"]["rss_kb_num"] / 1024 for x in pairs]
        updates = [x["B4"]["updates_num"] for x in pairs]
        incs = [x["B4"]["increase_updates_num"] for x in pairs]
        decs = [x["B4"]["decrease_updates_num"] for x in pairs]
        summaries.append({
            "update_mode": mode,
            "family": family,
            "update_every": ue,
            "cap": cap,
            "paired_runs": len(pairs),
            "median_updates": fmt(median(updates), 1),
            "median_increase_updates": fmt(median(incs), 1),
            "median_decrease_updates": fmt(median(decs), 1),
            "b2l_median_algorithm_ms": fmt(median(b2t)),
            "b3l_median_algorithm_ms": fmt(median(b3t)),
            "b4_median_algorithm_ms": fmt(median(b4t)),
            "b4_vs_b2l_median_speedup": fmt(median([
                x["B2L"]["algorithm_ns_num"] / x["B4"]["algorithm_ns_num"]
                for x in pairs if x["B4"]["algorithm_ns_num"]
            ])),
            "b4_vs_b3l_median_speedup": fmt(median([
                x["B3L"]["algorithm_ns_num"] / x["B4"]["algorithm_ns_num"]
                for x in pairs if x["B4"]["algorithm_ns_num"]
            ])),
            "b2l_median_rss_mib": fmt(median(b2r)),
            "b3l_median_rss_mib": fmt(median(b3r)),
            "b4_median_rss_mib": fmt(median(b4r)),
            "b2l_vs_b4_median_rss_ratio": fmt(median([
                x["B2L"]["rss_kb_num"] / x["B4"]["rss_kb_num"]
                for x in pairs if x["B4"]["rss_kb_num"]
            ])),
            "b3l_vs_b4_median_rss_ratio": fmt(median([
                x["B3L"]["rss_kb_num"] / x["B4"]["rss_kb_num"]
                for x in pairs if x["B4"]["rss_kb_num"]
            ])),
        })

    with (out / "summary.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=summary_fields)
        writer.writeheader()
        writer.writerows(summaries)

    complete_pairs = sum(len(v) for v in group_pairs.values())
    directions_ok = all(
        r["updates_num"] > 0 and r["increase_updates_num"] > 0 and r["decrease_updates_num"] > 0
        for r in valid
    ) if valid else False

    lines = [
        "# Crossover descriptive analysis",
        "",
        f"- Measurement rows: **{len(rows)}**",
        f"- Valid rows: **{len(valid)}**",
        f"- Complete matched B2L/B3L/B4 pairs: **{complete_pairs}**",
        f"- Every valid invocation contains both increase and decrease updates: **{'yes' if directions_ok else 'no'}**",
        "",
        "Speedup is comparator algorithm time divided by B4 algorithm time; values above 1 favor B4. "
        "RSS ratio is comparator peak RSS divided by B4 peak RSS; values above 1 mean B4 used less measured peak RSS.",
        "",
        "| mode | family | update every | cap | pairs | updates | B2L ms | B3L ms | B4 ms | B4/B2L speedup | B4/B3L speedup | B4 RSS MiB |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for s in summaries:
        lines.append(
            f"| {s['update_mode']} | {s['family']} | {s['update_every']} | {s['cap']} | "
            f"{s['paired_runs']} | {s['median_updates']} | {s['b2l_median_algorithm_ms']} | "
            f"{s['b3l_median_algorithm_ms']} | {s['b4_median_algorithm_ms']} | "
            f"{s['b4_vs_b2l_median_speedup']} | {s['b4_vs_b3l_median_speedup']} | "
            f"{s['b4_median_rss_mib']} |"
        )
    lines += [
        "",
        "These are descriptive medians from the recorded matched runs, not inferential confidence intervals or a state-of-the-art claim. "
        "The experiment matches resident-source capacity, not exact byte-level memory budgets.",
        "",
    ]
    (out / "ANALYSIS.md").write_text("\n".join(lines))


if __name__ == "__main__":
    main()
