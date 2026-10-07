# Crossover phase-diagram protocol

This experiment is deliberately **not** a speedup hunt. Its primary question is where, if anywhere, exact ADES becomes economically preferable to fresh exact bidirectional Dijkstra (B1), and where it does not.

## Independent axes

The sweep varies source distribution (uniform, Zipfian, single-hot, rotating-hot, hot-pool, adversarial churn), source-set size, and update interval. Five fixed seeds are retained for every cell. B1, B2, B3, and B4 consume deterministically regenerated identical traces.

The default grid includes no-update, sparse-update, moderate-update, and update-heavy regimes. Source reuse ranges from a single hot source to pools larger than the default resident cache.

## Falsification rules

Every completed cell is retained. No cell may be removed because ADES loses. The raw CSV is the primary evidence artifact. The summarizer labels a cell ADES-superior only when median B1/B4 >= 1.10, B1-superior only when <= 0.90, and otherwise parity. These thresholds are descriptive noise guards, not significance claims.

Claims must report the full phase surface, including: cold regimes where B1 wins; crossover cells; hot regimes where ADES wins; and update-heavy regimes where residency ceases to pay. Historical 2.6--4.8x observations are hypotheses to explain, not targets.

## Reproduction

Build normally, then run:

    benchmarks/run_phase_diagram.sh GRAPH results/phase.csv 10000

For publication evidence, increase trace length and seeds after the exploratory grid is frozen. Do not tune controller parameters on the evaluation seeds.

## Interpretation

The intended economic model is N_s C_Q > C_P + C_U. The experiment tests whether measured behavior exhibits that crossover without assuming that it must. A failure to find a stable crossover, controller thrashing under churn, or a B1-dominant surface is a valid refuting result and must remain in the evidence record.
