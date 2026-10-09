# PR49 Publication Viability Pilot — Analysis

## Decision: REFRAME

The pilot shows a nontrivial operating-region/crossover signal, but the scoped SOTA headline remains blocked until a DIRECT external comparator is qualified.

This is a pilot viability decision, not the definitive manuscript conclusion.

## Frozen decision facts

- DIRECT external comparators: 0
- Headline scoped-SOTA gate open: false
- Tested runtime-memory regimes: 36
- Crossover observed: true
- ADES fastest regimes: 3
- Material ADES wins vs FREQ-LRU-REPAIR: 2
- Material ADES losses vs FREQ-LRU-REPAIR: 0

## Fastest-profile counts

- COLD: 10
- B3L: 9
- FREQ-LRU-REPAIR: 5
- WORK-DEBT-REPAIR: 4
- ADES: 3
- WORK-LRU-REPAIR: 3
- FREQ-LRU-REBUILD: 2

## Strongest ADES-vs-simple cells

- syn-scale-free-50000-m4-v1 / churn / update 1/50 / budget 16777216: comparator/ADES=1.541482373 (95% CI 0.402966198–5.896692871)
- syn-scale-free-50000-m4-v1 / churn / update 1/50 / budget 67108864: comparator/ADES=1.518578408 (95% CI 0.369390559–6.242932643)
- ny-road-distance / churn / update 1/50 / budget 16777216: comparator/ADES=1.414020569 (95% CI 1.127228658–1.773778688)
- ny-road-distance / churn / update 1/5 / budget 16777216: comparator/ADES=1.349083799 (95% CI 1.234757643–1.473995408)
- syn-scale-free-50000-m4-v1 / churn / update 1/5 / budget 16777216: comparator/ADES=1.261282858 (95% CI 0.756352520–2.103297610)
- syn-scale-free-50000-m4-v1 / churn / update 1/5 / budget 67108864: comparator/ADES=1.249657769 (95% CI 0.776904007–2.010086865)
- syn-scale-free-50000-m4-v1 / uniform / update 1/50 / budget 16777216: comparator/ADES=1.021801133 (95% CI 0.961899145–1.085433500)
- syn-grid-224x224-v1 / uniform / update 1/5 / budget 67108864: comparator/ADES=1.008888965 (95% CI 0.992360069–1.025693168)

## Interpretation constraints

- A ratio above 1.0 in confidence.csv means ADES was faster than the named comparator.
- Confidence intervals are computed on seed-level mean log speedups; the two repetitions per seed measure runtime repeatability and are not treated as independent workload samples.
- With only three independent seeds, the pilot CI is intentionally conservative (t critical value for df=2).
- All losses and contradictory cells remain in measurements.csv and confidence.csv.
- C-SOTA cannot be declared while the PR48 DIRECT external comparator set is empty.
