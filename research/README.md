# Research archive

This directory contains scientifically useful development lineage that should not be mistaken for the current product surface.

ADES evolved through a sequence of benchmark, controller, measurement and publication-oriented experiments. Some original documents and frozen runner implementations contain pull-request numbers, old branch names, campaign language or version-specific terminology. Those identifiers are intentionally preserved **inside the research archive and immutable evidence packages** when they are needed to reconstruct what was run and why.

## What belongs here

- frozen experiment specifications and decision records;
- historical controller-evaluation runners retained for exact reproduction;
- publication-governance documents tied to a particular evidence campaign;
- negative or contradictory experimental findings;
- provenance references that would lose meaning if renumbered.

## What does not belong here

Current installation, architecture, API/CLI usage, routine testing, and the description of the implemented algorithm belong in the root README and `docs/`. Active operational entry points belong in `tools/` and use evergreen names.

## Preserved evidence outside this directory

The existing `evidence/` and checked-in `results/` packages remain in place because their directory names, manifests, checksums and cross-references are part of their provenance. Pull-request or workflow-run identifiers inside those packages are historical scientific identifiers, not current implementation milestones.

## Historical documents

`research/history/` collects design/evaluation documents that were previously mixed into the product documentation. Filenames are purpose-oriented; document bodies may still state their original campaign identifiers so a reader can map them back to Git history and archived runs.

## Frozen runners

`research/history/runners/` preserves the exact legacy Python drivers for historical publication-pilot and economic-controller experiments. Evergreen wrappers in `tools/` delegate to these implementations. This keeps current commands professional without silently rewriting frozen experimental logic or manifest schemas.

## Publication protocols

`research/publication/` contains frozen publication-facing protocols whose wording is evidence-governance history rather than current user documentation. Current benchmark guidance is in `docs/benchmark-methodology.md` and `docs/reproducibility.md`.

## Rule for historical identifiers

Do not remove or rename an identifier when it is required to reproduce an archived result, verify a checksum, resolve a workflow/artifact reference, or understand a preregistered decision. Do remove chronology from current-facing names and prose when it has no scientific function.
