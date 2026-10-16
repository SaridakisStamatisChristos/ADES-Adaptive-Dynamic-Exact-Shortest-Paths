# PR82 — VeloGraphX DIRECT registry reconciliation

## Status

**REGISTRY RECONCILIATION ONLY.**

PR82 does not create a new qualification, change the frozen Publication Contract v1.0, alter ADES, modify upstream VeloGraphX, change benchmark semantics, or promote a performance result after the fact.

It repairs one bookkeeping lag identified by PR81: `benchmarks/comparators/registry.json` predated the completed PR64→PR65→PR66→PR80 VeloGraphX qualification/evidence chain and therefore still listed only Boost.Graph as `DIRECT`.

## Claims addressed

```text
claim_ids: [C4, C-SOTA]
```

## Requirement ledger

| ID | Requirement | Acceptance evidence | State |
|---|---|---|---|
| R82-1 | Preserve Publication Contract v1.0 and frozen hard-gate names/order | unchanged `docs/PUBLICATION_CONTRACT.md`; unchanged validator hard gates | VERIFIED |
| R82-2 | Preserve all pre-existing comparator entries and statuses | registry diff may add VeloGraphX but must not silently promote/demote old entries | VERIFIED by review/CI |
| R82-3 | Add VeloGraphX only if every DIRECT hard gate has executed evidence | PR64 semantic qualification + PR65 adapter qualification | VERIFIED |
| R82-4 | Preserve adapter-mediated scope | registry text must state upstream is fixed-source and arbitrary-source support is supplied by the transparent bounded-LRU adapter | VERIFIED |
| R82-5 | Preserve reproducible implementation identity | upstream commit + primary path + Apache-2.0 license + adapter/pair-driver identities in later evidence | VERIFIED |
| R82-6 | Registry validator must accept VeloGraphX as DIRECT without weakening validator rules | `tools/validate_comparator_registry.py` on PR82 registry | OPEN until CI |
| R82-7 | No new SOTA performance claim is created by this PR | only qualification bookkeeping is changed | VERIFIED |

## Evidence chain supporting DIRECT status

### PR64 — semantic prequalification

Pinned upstream:

- repository: `sauravsingla/VeloGraphX`;
- commit: `2aa84ce81a180487239c661b65639e5100136075`;
- upstream source modified: **no**.

Canonical run: `38712739826`.

Artifact:

- ID `11960558666`;
- SHA-256 `d376779c3fc149bb518c42606b5b00acd31f9e7de33fdb76bb9af7220e8ac733`.

Seven independently Bellman-Ford-checked semantic cases passed, including:

- increase then decrease;
- zero-weight/equal-distance alternatives;
- asymmetric directed arcs;
- nonzero source;
- removal fallback;
- deterministic 200-step mixed updates;
- two independent source instances receiving identical updates.

PR64 correctly stopped at `PENDING_ADAPTER_MEMORY_TIMING_AND_TRACE_QUALIFICATION`.

### PR65 — adapter and DIRECT qualification

Canonical run: `38728494288`.

Job: `116400131724`.

Artifact:

- ID `11969691275`;
- SHA-256 `88381ab92e6c9df769d9efe849d142813afe5dfe646ec1e2f1837e901180f588`.

PR65 supplied the missing DIRECT gates:

- canonical matched trace replay;
- exact `ADES_ORACLE_V2` parity;
- arbitrary-source operation through online causal byte-bounded LRU;
- no future knowledge;
- full logical accounting for every retained private VeloGraphX graph + SSSP state;
- hard 64 MiB persistent-state enforcement;
- declared timing boundaries including source-instance creation/reconstruction;
- exact directed mixed weight increases/decreases;
- targeted conflicting-parallel-edge falsifier;
- source/implementation pinning and provenance.

The adapter does **not** rewrite upstream VeloGraphX. It converts ADES edge-ID semantics to VeloGraphX endpoint-keyed simple-graph semantics by maintaining the exact endpoint minimum for nonnegative parallel arcs and propagating a VeloGraphX update only when that minimum changes.

PR65's explicit classification was:

`VeloGraphX is now DIRECT-qualified through the explicit PR65 arbitrary-source adapter for the tested ADES weighted edge-update contract.`

## Later falsification/holdout evidence

DIRECT qualification does not depend on winning performance. Nevertheless, later experiments tested whether the adapter qualification remained meaningful outside the qualification traces.

### PR66 — fresh road/source-reuse holdout

- 24/24 exact cells;
- independent oracle agreement;
- 64 MiB logical state;
- heavy source-reuse cells exercised actual VeloGraphX cache hits;
- no total/query loss cells;
- canonical run `38730045262`;
- artifact `11971060916`.

### PR80 — fresh non-road cross-product holdout

- 24/24 exact cells;
- independent ADES-Dijkstra = Boost.Graph-Dijkstra oracle agreement;
- three fresh preferential-attachment graph instances;
- uniform, hot-pool-2, hot-pool-4, rotating-hot-4 source regimes;
- 64 MiB logical state;
- 996 VeloGraphX cache hits;
- canonical repaired run `38856829047`;
- artifact `12027088809`;
- artifact digest `sha256:617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100`.

These runs support the external validity of the adapter-mediated comparison but are not required to transform PR65's qualification into truth by performance outcome.

## Frozen hard-gate mapping

| Frozen hard gate | VeloGraphX evidence |
|---|---|
| exact_answers | PR64 semantic cases; PR65 canonical oracle parity; PR66/80 holdouts |
| online_operations | PR64/65 online update replay |
| directed_graphs | PR64 asymmetric directed case; PR65 directed NY representation |
| nonnegative_weighted_graphs | pinned weighted SSSP semantics + semantic probes |
| weight_increases | PR64 + PR65 mixed update qualification |
| weight_decreases | PR64 + PR65 mixed update qualification |
| no_future_knowledge | PR65 causal LRU and online replay |
| arbitrary_query_sources_supported | PR65 explicit adapter |
| matched_trace_adapter | PR65 canonical trace replay |
| independent_oracle_validation | PR64 Bellman-Ford; PR65 ADES_ORACLE_V2; PR66/80 Boost cross-oracle |
| persistent_memory_budget_enforceable | PR65 hard 64 MiB gate |
| persistent_memory_accounting_auditable | PR65 graph+SSSP per-source logical accounting + separate RSS |
| preprocessing_semantics_declared | PR65 adapter construction and timing disclosures |
| timing_boundary_declared | PR65 matched pair driver |
| implementation_pinned | exact upstream commit and frozen adapter identities |
| license_compatible_for_reproduction | upstream Apache-2.0 |

## Important scope statement

`DIRECT` means **admissible direct evidence under the frozen matched contract**. It does not mean:

- upstream VeloGraphX natively provides arbitrary-source queries;
- VeloGraphX and ADES have equal process RSS;
- the adapter is performance-neutral;
- ADES wins every possible graph/workload;
- VeloGraphX is the only credible dynamic-maintenance comparator;
- qualifying VeloGraphX automatically proves C-SOTA.

The arbitrary-source comparison is explicitly adapter-mediated. The adapter's source creation, retained state, update propagation, admission/eviction, and timing are charged and disclosed.

## PR82 acceptance rule

PR82 is accepted only if:

1. the unchanged comparator validator passes;
2. `velographx-weighted-sssp-adapter` appears in `direct_external_comparators`;
3. Boost.Graph remains DIRECT;
4. existing PENDING/CONTEXT/INELIGIBLE entries remain present;
5. all 16 VeloGraphX hard gates equal exactly `PASS`;
6. the adapter blob remains `b09a9f51b2b0300dce2801d0a8afaa49440d45a6`;
7. the pair-driver blob remains `44019d478377282b5056a6b07d86d76757f7efc0`;
8. no ADES performance source is modified by PR82.

If any gate fails, VeloGraphX must not remain DIRECT merely to preserve PR81's preferred conclusion.