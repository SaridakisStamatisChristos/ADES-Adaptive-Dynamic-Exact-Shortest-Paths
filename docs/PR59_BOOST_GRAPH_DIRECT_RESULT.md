# PR59 — Boost.Graph Dijkstra external DIRECT baseline result

## Disposition

**DIRECT under frozen comparator contract v1.0 as an external exact recomputation baseline.**

This is deliberately **not** a claim that Boost.Graph Dijkstra is a dynamic-maintenance algorithm or the strongest available dynamic competitor. It qualifies because the frozen DIRECT contract permits an online exact comparator that consumes the same update/query stream without future knowledge and satisfies every fairness/evidence gate.

Pinned implementation:

- Boost release: `1.92.0`
- Boost.Graph commit: `fc5dac2c552f945846449fd123a9b8335d5c063a`
- official Boost 1.92.0 source archive SHA-256: `5c1d40cb8e19adbf740a4ec2da35b3e58f3f5804b1dce44deb53df72193cbc6c`
- license: Boost Software License 1.0

## Evidence lineage

Qualification workflow run: `38696600802`

### Pinned semantic preflight

Job: `116294497164`

Artifact:

- ID: `11955145188`
- SHA-256: `392b297659bfd8e2949537fa5eb9d0681e0fb9bd95979e6c7d421ce4e3fc5f44`

The Boost 1.92.0 implementation was checked against an independent Bellman–Ford oracle on directed nonnegative graphs containing zero weights, equal-distance alternatives, asymmetric reverse arcs, mixed increases/decreases, and arbitrary query sources.

Result:

- 132 online weight updates
- 70 increases
- 62 decreases
- 414 source-distance oracle comparisons
- 4/4 cases PASS
- 0 mismatches

This establishes the problem-domain semantic gates without using Boost Dijkstra as its own oracle.

### Matched DIRECT adapter

Job: `116294797255`

Artifact:

- ID: `11954231148`
- SHA-256: `327fe01a7303f3d46ce4a054d5898324603e8e1e8b5084000ee9eb07e0709ba5`

The adapter consumes the exact PR42 canonical trace format and the reusable `ADES_ORACLE_V2` bundle. It validates trace/oracle SHA-256 identity before execution and compares every query result against the publication oracle outside the timed algorithm boundary.

Two committed NY DIMACS graph variants were executed independently.

## Matched evidence

### USA-road-d.NY.gr.gz

- graph SHA-256: `7b2446c7ffe6179efbc42af6812448e8cb782f12968ca2bb1d50d979343056d4`
- canonical trace SHA-256: `f03efd5b0f410dedbf2e991fe73dbe45ca525e66dd1526ca48d059ef0b175a21`
- operations: 120
- queries: 91
- updates: 29
- increases: 1
- decreases: 28
- unique query sources: 91
- exactness: PASS
- graph load: 187,435,074 ns
- declared preprocessing / graph conversion: 28,141,457 ns
- query time: 4,943,965,062 ns
- update time: 74,834,199 ns
- algorithm time: 5,018,799,261 ns
- accounted persistent algorithm state: 0 bytes
- peak accounted persistent algorithm state: 0 bytes
- enforced persistent-state budget: 1 byte
- accounting model: PR43 v1

### USA-road-t.NY.gr.gz

- graph SHA-256: `de902573142dbf12c42b065d9a6a679e4794372eb91d2748ee78089720377156`
- canonical trace SHA-256: `c04200d8cfa321635dfe6c2c72af802c844dbfd927978e5c2bdf4463d87bfe25`
- operations: 120
- queries: 91
- updates: 29
- increases: 0
- decreases: 29
- unique query sources: 91
- exactness: PASS
- graph load: 177,121,199 ns
- declared preprocessing / graph conversion: 26,568,114 ns
- query time: 5,007,054,083 ns
- update time: 72,471,922 ns
- algorithm time: 5,079,526,005 ns
- accounted persistent algorithm state: 0 bytes
- peak accounted persistent algorithm state: 0 bytes
- enforced persistent-state budget: 1 byte
- accounting model: PR43 v1

The second matched trace happened to contain only decreases. The increase gate is therefore not inferred from that cell; it is established independently by the pinned semantic preflight, which executed 70 increases and 62 decreases with zero mismatches.

## Memory-accounting rationale

PR43 accounting v1 excludes common graph input storage, benchmark-oracle state, harness state, and temporary query workspaces. The Boost recomputation adapter retains no shortest-path tree, distance cache, source cache, admission state, repair model, or other adaptive state between operations.

Therefore:

```text
accounted_algorithm_state_bytes = 0
peak_accounted_algorithm_state_bytes = 0
```

The matched workflow deliberately sets:

```text
ADES_PERSISTENT_STATE_BUDGET_BYTES=1
```

and hard-fails if the reported peak exceeds that budget. Process peak RSS is retained separately in the evidence artifact and is not substituted for the PR43 fairness variable.

## Timing boundary

The PR44-compatible algorithm boundary includes only:

- the fresh Boost.Graph Dijkstra call for each QUERY, including its temporary distance workspace;
- the online edge-id lookup and weight mutation for each UPDATE.

It excludes:

- graph file loading;
- trace parsing/validation;
- oracle parsing/validation;
- exactness comparison;
- graph conversion.

Graph conversion is reported separately as `preprocessing_ns`; graph loading is reported separately as `graph_load_ns`.

## Frozen DIRECT gates

| Gate | Result |
|---|---|
| exact answers | PASS |
| online operations | PASS |
| directed graphs | PASS |
| nonnegative weighted graphs | PASS |
| weight increases | PASS |
| weight decreases | PASS |
| no future knowledge | PASS |
| arbitrary query sources | PASS |
| matched trace adapter | PASS |
| independent oracle validation | PASS |
| persistent-memory budget enforceable | PASS |
| persistent-memory accounting auditable | PASS |
| preprocessing semantics declared | PASS |
| timing boundary declared | PASS |
| implementation pinned | PASS |
| license compatible for reproduction | PASS |

## Claim boundary

PR59 removes the literal **zero-DIRECT-external-comparator** blocker by qualifying a reproducible external exact recomputation baseline.

It does **not** establish a broad claim that ADES is the best dynamic shortest-path system. For a strong practical dynamic-SOTA claim, the campaign should still qualify and compare against at least one credible external dynamic-maintenance implementation if one can satisfy the frozen assumptions. The newly rediscovered `felipe31/Ramalingam-Reps` implementation is a candidate for that next investigation, but its fixed-source and insertion/removal interface require qualification before it can be used.
