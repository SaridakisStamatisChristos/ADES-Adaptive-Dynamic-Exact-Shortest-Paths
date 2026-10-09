# PR50 Predictive Economic Residency Evidence

This directory permanently preserves the first frozen ADES-v2 development + holdout evaluation.

- Source commit: `3fb5f57a19c477742c0a87bac6f901f53b37d732`
- GitHub Actions run: `37892264840`
- Full artifact: `ades-pr50-predictive-economic-full`
- Development matrix: 36 PR49-derived regimes (tuning/regression evidence only)
- Fresh holdout: 72 regimes using unseen graph classes, locality families, seeds, and update modes

The holdout is preserved before any post-result controller tuning. Negative evidence is retained: the frozen ADES-v2 has one statistically material holdout regression under the predeclared >=10% rule.

Browsable tables are committed directly. `compact-evidence.tgz` additionally contains all canonical workload traces and exact oracle bundles used by the development and holdout matrices.
