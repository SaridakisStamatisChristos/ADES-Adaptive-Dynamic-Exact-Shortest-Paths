#!/usr/bin/env python3
"""Run the frozen fast-economic residency evaluation.

The underlying driver is archived unchanged under research/history/runners so
historical manifests and experiment semantics remain reproducible.
"""
from pathlib import Path
import runpy

DRIVER = Path(__file__).resolve().parents[1] / "research" / "history" / "runners" / "fast_economic_v3.py"
runpy.run_path(str(DRIVER), run_name="__main__")
