#!/usr/bin/env python3
"""Run the frozen publication-viability pilot.

The historical driver is archived unchanged so its manifest schema and
preregistered experiment semantics remain reproducible.
"""
from pathlib import Path
import runpy

DRIVER = Path(__file__).resolve().parents[1] / "research" / "history" / "runners" / "publication_viability_pilot_v1.py"
runpy.run_path(str(DRIVER), run_name="__main__")
