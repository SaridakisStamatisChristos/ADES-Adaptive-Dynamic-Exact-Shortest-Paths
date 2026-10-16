#!/usr/bin/env python3
"""PR77 adapter for the frozen PR70 fixed-source performance harness.

The workload, correctness oracle, timing extraction, repetitions, bootstrap, and
performance rule remain implemented by run_pr70_graphbolt_performance.py.
PR77 changes only the GraphBolt execution substrate to the PR76-qualified
historical Cilk Plus wrapper and relabels the resulting evidence so it cannot be
confused with PR70 OpenMP evidence.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

import run_pr70_graphbolt_performance as core

CILK_IMAGE = "gcc@sha256:1938a2199146f37d7c5b697a66d4379b75e3cd6ca5e08380c6803ab25e0ba9a4"
PR76_RUN = 38854118754
PR76_JOB = 116821640368
PR76_ARTIFACT = 12026410989
PR76_ARTIFACT_DIGEST = "sha256:56fd8c8cadc3ed41414f3b1bdbefe62a8aebd15f81c261ef2742720b0acc0166"

_original_run_process = core.run_process
_original_timed_graphbolt = core.timed_graphbolt


def _run_process_without_host_preload(cmd: list[str], env: dict[str, str], timeout: int):
    """Keep mimalloc inside the pinned Cilk container, not in the host shell."""
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
        raise SystemExit("PR77 requires --out") from exc


def _rewrite_summary(out: Path) -> None:
    path = out / "summary.json"
    if not path.exists():
        return
    d = json.loads(path.read_text())
    old = d.get("disposition")
    mapping = {
        "SUPPORTS_ADES_RESIDENT_FIXED_SOURCE_CORE_SUPERIORITY_ON_PR70_TESTED_SLICE":
            "SUPPORTS_ADES_RESIDENT_FIXED_SOURCE_CORE_SUPERIORITY_OVER_HISTORICAL_CILK_GRAPHBOLT_ON_PR77_TESTED_SLICE",
        "FALSIFIES_PREFERRED_PR70_SUPERIORITY_HYPOTHESIS_ON_TESTED_SLICE":
            "FALSIFIES_PREFERRED_PR77_CILK_SUPERIORITY_HYPOTHESIS_ON_TESTED_SLICE",
        "REJECTED_CORRECTNESS_OR_DISTANCE_BOUND":
            "REJECTED_PR77_CORRECTNESS_OR_DISTANCE_BOUND",
    }
    if old not in mapping:
        raise SystemExit(f"unexpected inherited PR70 disposition: {old!r}")
    d["schema"] = "ades-pr77-graphbolt-historical-cilk-fixed-source-performance-v1"
    d["contract"] = "docs/PR77_GRAPHBOLT_CILK_FIXED_SOURCE_PERFORMANCE_CONTRACT.md"
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
    d["claim_boundary"] = (
        "resident fixed-source dynamic SSSP core versus the PR76-qualified "
        "historical-Cilk GraphBolt comparator on the frozen PR70-derived synthetic directed slice only"
    )
    path.write_text(json.dumps(d, indent=2, sort_keys=True) + "\n")


def main() -> int:
    out = _out_dir_from_argv()
    core.run_process = _run_process_without_host_preload
    core.timed_graphbolt = _timed_cilk_graphbolt
    rc = core.main()
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
        }, indent=2, sort_keys=True))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
