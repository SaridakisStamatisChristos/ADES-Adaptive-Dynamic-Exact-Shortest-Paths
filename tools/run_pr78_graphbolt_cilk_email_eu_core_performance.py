#!/usr/bin/env python3
"""PR78 adapter: run the frozen PR72 email-Eu-core experiment with PR76/77 historical Cilk GraphBolt.

PR78 deliberately reuses PR72's topology parser, deterministic weights, source
scenarios, logical replacements, correctness oracle, timing extraction,
repetitions, bootstrap, and performance decision rule. The only comparator
change is GraphBolt's execution substrate: historical Cilk Plus with four workers
inside the digest-pinned legacy container qualified by PR76 and exercised by PR77.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

import run_pr72_email_eu_core_performance as experiment

CILK_IMAGE = "gcc@sha256:1938a2199146f37d7c5b697a66d4379b75e3cd6ca5e08380c6803ab25e0ba9a4"
PR76_RUN = 38854118754
PR76_JOB = 116821640368
PR76_ARTIFACT = 12026410989
PR76_ARTIFACT_DIGEST = "sha256:56fd8c8cadc3ed41414f3b1bdbefe62a8aebd15f81c261ef2742720b0acc0166"
PR77_RUN = 38854540814
PR77_JOB = 116822953219
PR77_ARTIFACT = 12026740963
PR77_ARTIFACT_DIGEST = "sha256:d31a308025272a667a224e2c5b781e2039df9339bfebab7d9e35c2e33650f042"

core = experiment.core
_original_run_process = core.run_process
_original_timed_graphbolt = core.timed_graphbolt


def _run_process_without_host_preload(cmd: list[str], env: dict[str, str], timeout: int):
    """Keep the pinned mimalloc preload inside the Cilk container only."""
    adjusted = dict(env)
    if cmd and "run_graphbolt_cilk" in cmd[0]:
        adjusted.pop("LD_PRELOAD", None)
        adjusted.pop("OMP_NUM_THREADS", None)
        adjusted["CILK_NWORKERS"] = "4"
    return _original_run_process(cmd, adjusted, timeout)


def _timed_cilk_graphbolt(exe: str, case: dict, env: dict[str, str]) -> dict:
    row = _original_timed_graphbolt(exe, case, env)
    row["implementation"] = "GraphBolt-portability-patched-historical-Cilk4"
    return row


def _out_dir_from_argv() -> Path:
    try:
        i = sys.argv.index("--out")
        return Path(sys.argv[i + 1])
    except (ValueError, IndexError) as exc:
        raise SystemExit("PR78 requires --out") from exc


def _rewrite_summary(out: Path) -> None:
    path = out / "summary.json"
    if not path.exists():
        return
    d = json.loads(path.read_text())
    old = d.get("disposition")
    mapping = {
        "SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_ON_PR72_EMAIL_EU_CORE_SLICE":
            "SUPPORTS_ADES_FIXED_SOURCE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR78_EMAIL_EU_CORE_SLICE",
        "FALSIFIES_PREFERRED_PR72_EXTERNAL_TOPOLOGY_HYPOTHESIS_ON_TESTED_SLICE":
            "FALSIFIES_PREFERRED_PR78_CILK_EMAIL_EU_CORE_HYPOTHESIS_ON_TESTED_SLICE",
        "REJECTED_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND":
            "REJECTED_PR78_CORRECTNESS_TOPOLOGY_OR_DISTANCE_BOUND",
    }
    if old not in mapping:
        raise SystemExit(f"unexpected inherited PR72 disposition: {old!r}")

    d["schema"] = "ades-pr78-graphbolt-historical-cilk-email-eu-core-performance-v1"
    d["contract"] = "docs/PR78_GRAPHBOLT_CILK_EMAIL_EU_CORE_PERFORMANCE_CONTRACT.md"
    d["disposition"] = mapping[old]
    d.pop("graphbolt_threads", None)
    d["graphbolt_parallel_mode"] = "historical Cilk Plus"
    d["graphbolt_workers"] = 4
    d["graphbolt_cilk_image"] = CILK_IMAGE
    d["pr76_semantic_evidence"] = {
        "run": PR76_RUN,
        "job": PR76_JOB,
        "artifact": PR76_ARTIFACT,
        "artifact_digest": PR76_ARTIFACT_DIGEST,
        "exact_cases": 9,
        "checks": 729,
    }
    d["pr77_historical_cilk_performance_lineage"] = {
        "run": PR77_RUN,
        "job": PR77_JOB,
        "artifact": PR77_ARTIFACT,
        "artifact_digest": PR77_ARTIFACT_DIGEST,
        "disposition": "SUPPORTS_ADES_RESIDENT_FIXED_SOURCE_CORE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR77_TESTED_SLICE",
    }
    d["claim_boundary"] = (
        "one frozen real external directed SNAP email-Eu-core topology with deterministic "
        "synthetic positive weights and four fixed SCC-source scenarios, versus the "
        "PR76-qualified historical-Cilk GraphBolt comparator only"
    )
    path.write_text(json.dumps(d, indent=2, sort_keys=True) + "\n")


def main() -> int:
    out = _out_dir_from_argv()
    core.run_process = _run_process_without_host_preload
    core.timed_graphbolt = _timed_cilk_graphbolt
    rc = experiment.main()
    _rewrite_summary(out)
    if (out / "summary.json").exists():
        d = json.loads((out / "summary.json").read_text())
        print(json.dumps({
            "schema": d.get("schema"),
            "disposition": d.get("disposition"),
            "graphbolt_parallel_mode": d.get("graphbolt_parallel_mode"),
            "graphbolt_workers": d.get("graphbolt_workers"),
            "maintenance": d.get("maintenance"),
            "total": d.get("total"),
            "scenario_level": d.get("scenario_level"),
        }, indent=2, sort_keys=True))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
