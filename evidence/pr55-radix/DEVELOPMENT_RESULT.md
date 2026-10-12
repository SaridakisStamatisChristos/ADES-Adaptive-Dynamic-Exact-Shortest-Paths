# PR55 radix-queue ablation — development result

## Evidence classification

**Development ablation.** PR54 is the causal baseline; this is not a fresh publication holdout and does not establish universal SOTA.

- PR54 baseline head: `1f2452fc58e25ecb5d5ab68a5c2122766586f8bc`
- PR55 candidate head measured: `f1e56fc802a778d5916bacd64750e8db75df4ae9`
- GitHub Actions run: `38223803096`
- Artifact ID: `11735563036`
- Artifact SHA-256: `2fd0a9401e1c694214b878ae547a59cb50b5ce6dd46dca046ae6b26fc967f872`
- Independent seeds: `7, 17, 29`
- Repetitions per seed: `2`, with PR54/PR55 execution order reversed on repetition 2
- Graph classes: grid, scale-free, clustered
- Workloads per graph: hot-pool/update-1-in-10 and churn/update-1-in-5
- Profiles: COLD, FREQ-LRU-REPAIR, ADES-V3
- Exact trace identity, baseline-generated oracle agreement, and equal byte-budget enforcement were hard requirements.

`pr54_over_pr55_geomean > 1` means the radix candidate is faster.

## Results

| case | profile | PR54 / PR55 | 95% CI | verdict |
|---|---|---:|---:|---|
| grid-hotpool-u10 | COLD | 1.194822× | 1.109908–1.286231 | IMPROVEMENT |
| grid-hotpool-u10 | FREQ-LRU-REPAIR | 1.182601× | 1.088764–1.284526 | IMPROVEMENT |
| grid-hotpool-u10 | ADES-V3 | 1.159035× | 1.146963–1.171234 | IMPROVEMENT |
| grid-churn-u5 | COLD | 1.176292× | 1.153999–1.199016 | IMPROVEMENT |
| grid-churn-u5 | FREQ-LRU-REPAIR | 1.147011× | 1.115638–1.179266 | IMPROVEMENT |
| grid-churn-u5 | ADES-V3 | 1.187619× | 0.938766–1.502438 | INCONCLUSIVE |
| scale-free-hotpool-u10 | COLD | 1.467097× | 1.346286–1.598749 | IMPROVEMENT |
| scale-free-hotpool-u10 | FREQ-LRU-REPAIR | 1.411048× | 1.360736–1.463221 | IMPROVEMENT |
| scale-free-hotpool-u10 | ADES-V3 | 1.421103× | 1.135583–1.778412 | IMPROVEMENT |
| scale-free-churn-u5 | COLD | 1.421592× | 1.312826–1.539369 | IMPROVEMENT |
| scale-free-churn-u5 | FREQ-LRU-REPAIR | 1.496054× | 1.303266–1.717359 | IMPROVEMENT |
| scale-free-churn-u5 | ADES-V3 | 1.510539× | 1.372602–1.662336 | IMPROVEMENT |
| clustered-hotpool-u10 | COLD | 1.370347× | 1.303173–1.440985 | IMPROVEMENT |
| clustered-hotpool-u10 | FREQ-LRU-REPAIR | 1.416183× | 1.266363–1.583728 | IMPROVEMENT |
| clustered-hotpool-u10 | ADES-V3 | 1.372089× | 1.244686–1.512532 | IMPROVEMENT |
| clustered-churn-u5 | COLD | 1.375983× | 1.261257–1.501145 | IMPROVEMENT |
| clustered-churn-u5 | FREQ-LRU-REPAIR | 1.373900× | 1.264809–1.492400 | IMPROVEMENT |
| clustered-churn-u5 | ADES-V3 | 1.362999× | 1.134649–1.637306 | IMPROVEMENT |

Machine result: **17 statistically supported improvements, 0 regressions, 1 inconclusive**.

## Inconclusive cell analysis

The single inconclusive cell (`grid-churn-u5 / ADES-V3`) is uncertainty in effect size, not a measured loss. Every one of the six paired executions favored PR55:

- seed 7 repeat ratios PR54/PR55: `1.1292×`, `1.0927×`;
- seed 17: `1.1118×`, `1.1677×`;
- seed 29: `1.3094×`, `1.3377×`.

The wide three-seed t interval comes from heterogeneous gain magnitude across only three independent seeds.

Queue tie ordering also changed the selected exact SPT in at least one regime, which can change later repair economics without changing shortest-path answers. In grid churn seed 29, PR55 actually avoided one PR54 repair abort/full rebuild and completed one additional local increase repair. That behavior was favorable in the measured pair and is retained as part of the whole-strategy ablation rather than normalized away.

## Correctness evidence

PR55 includes direct radix-queue tests for:

- equal keys;
- monotone ordering;
- reset/clear semantics;
- rejection of a key below the last extracted key;
- randomized comparison against `std::priority_queue` with monotone dynamic inserts.

The full repository exactness/differential suite also passes with radix queues in:

- full Dijkstra builds;
- bidirectional cold search;
- decrease repair;
- boundary-seeded restricted increase repair.

## Disposition

**ACCEPT the radix queue as the stronger priority-kernel candidate, subject to the normal final compatibility/NY gate completing green.**

The magnitude and breadth of the gains justify preserving PR55 rather than attempting a topology-specific heap switch. The evidence gives no current reason for a brittle graph classifier: one common radix strategy wins across all three tested topology classes and all three profiles.

A fresh post-freeze holdout is still required before a stronger performance claim.
