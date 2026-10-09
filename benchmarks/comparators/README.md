# External comparator registry

`registry.json` is the machine-readable PR48 qualification ledger for external shortest-path methods.

The authoritative eligibility rules are in `docs/COMPARATOR_ELIGIBILITY.md`.

## Status meaning

- `DIRECT` — admissible in matched C-SOTA performance/Pareto evidence.
- `PENDING` — potentially direct, but qualification evidence is incomplete.
- `CONTEXT` — relevant comparison with materially different assumptions.
- `INELIGIBLE` — outside the frozen direct-comparison problem domain.

A method may be promoted to `DIRECT` only when **every** frozen hard gate in the registry equals the literal string `PASS`. Theory-only evidence, an unpinned implementation, or a missing trace/memory adapter cannot be waived by prose.

Validate locally with:

```bash
python3 tools/validate_comparator_registry.py \
  --registry benchmarks/comparators/registry.json \
  --output results/comparator-qualification/SUMMARY.json
```

The validator intentionally permits zero DIRECT external comparators, but then emits:

```text
headline_sota_ready = false
```

That state blocks any claim that PR48 itself established external-comparator SOTA readiness.
