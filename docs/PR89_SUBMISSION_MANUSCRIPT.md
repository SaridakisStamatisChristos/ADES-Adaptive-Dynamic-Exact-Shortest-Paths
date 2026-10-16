# PR89 — Citation-complete submission manuscript

## Decision target

PR89 completes the manuscript half of the PR87/PR88 publication handoff without changing the frozen ADES candidate, comparator implementations, workloads, benchmark outputs, publication contract, or prior-art verdict.

The intended disposition is:

- **performance:** `GO — SCOPED PRACTICAL SOTA EVIDENCE` under Publication Contract v1.0;
- **broad algorithmic novelty:** `NO_GO`;
- **integrated-system novelty:** `UNESTABLISHED / POSSIBLY DISTINCT`;
- **artifact provenance:** `PR88 INDEPENDENT PER-CELL RECOMPUTATION PASS`;
- **manuscript state:** citation-complete venue-neutral submission source.

PR89 does not create a stronger scientific claim. It converts the already-audited scaffold into a literature-grounded paper surface and machine-checks that the conversion did not change the evidence boundary.

## New publication surfaces

1. `paper/ADES_MANUSCRIPT.md`
   - citation-complete venue-neutral manuscript;
   - full problem model, architecture, correctness argument, related work, methodology, results, negative evidence, threats to validity, reproducibility map, and conclusion;
   - preserves the historical `paper/ADES_MANUSCRIPT_SCAFFOLD.md` unchanged.

2. `paper/references.bib`
   - revalidated bibliography for the prior-art claims used in the manuscript;
   - includes exact DOI metadata where available.

3. `.github/workflows/pr89-submission-manuscript.yml`
   - resolves every manuscript citation key against the bibliography;
   - rejects duplicate/missing DOI identities;
   - cross-checks central result values and the frozen candidate against `evidence/pr87_publication_manifest.json`;
   - pins the PR88 reproduction run/artifact/digest;
   - enforces the novelty, adapter, memory, statistics, and universal-SOTA boundaries;
   - proves the PR is manuscript-only.

## Bibliographic revalidation

Revalidation was performed on 2026-10-16 against publisher, proceedings, institutional, or established bibliographic records. The manuscript bibliography includes:

| Key | Work | Verified identity |
|---|---|---|
| `ramalingam1996incremental` | Ramalingam & Reps, incremental generalized shortest path | DOI `10.1006/jagm.1996.0046`, *Journal of Algorithms* 21(2):267–305 |
| `frigioni2000fully` | Frigioni, Marchetti-Spaccamela & Nanni, fully dynamic SPT | DOI `10.1006/jagm.1999.1048`, *Journal of Algorithms* 34(2):251–281 |
| `king2001space` | King & Thorup, dynamic shortest-path space-saving trick | DOI `10.1007/3-540-44679-6_30`, COCOON 2001, LNCS 2108:268–277 |
| `tripathi2018incremental` | Tripathi et al., transactional incremental dynamic graph queries | DOI `10.1002/cpe.4500`, *Concurrency and Computation: Practice and Experience* 30(18):e4500 |
| `jensen2012effective` | Jensen, Thomsen & Yiu, shortest-path caching | DOI `10.1145/2213836.2213872`, SIGMOD 2012:313–324 |
| `li2017refreshment` | Li et al., shortest-path cache refresh after edge change | DOI `10.1016/j.eswa.2016.08.011`, *Expert Systems with Applications* 67:1–11 |
| `ammar2022optimizing` | Ammar et al., memory-bounded differential graph-query maintenance | DOI `10.14778/3551793.3551862`, PVLDB 15(11):3186–3198 |
| `einziger2017tinylfu` | Einziger, Friedman & Manes, TinyLFU | DOI `10.1145/3149371`, *ACM Transactions on Storage* 13(4), Article 35 |
| `einziger2023access` | Einziger, Himelbrand & Waisbard, access-time-aware caching | DOI `10.1145/3572778`, *ACM Transactions on Storage* 19(1), Article 8 |
| `gupta1999selection` | Gupta & Mumick, materialization under maintenance cost | DOI `10.1007/3-540-49257-7_28`, ICDT'99, LNCS 1540:453–470 |
| `bok2022cost` | Bok et al., cost-based incremental/static graph processing | DOI `10.3390/electronics11040660`, *Electronics* 11(4):660 |
| `rai2026alsssp` | Rai, ALSSSP contemporary adaptive hot-source SSSP work | DOI `10.5281/zenodo.19184398`; retained as a **contemporary collision**, not pre-ADES prior art |
| `siek2002boost` | Siek, Lee & Lumsdaine, *The Boost Graph Library* | Addison-Wesley, 2002 |

The Rai/ALSSSP entry remains intentionally classified as contemporary priority-relevant work. Its presence cannot be used to establish pre-ADES prior art or resolve priority.

## Evidence inherited without reinterpretation

### PR62 — Boost.Graph NY

- run `38711826306`;
- artifact `11961213712`;
- ZIP SHA-256 `0bd64d43ba48fda4c9e0752ea6acafc1c9fa8ddb7eec2f2fdabc88d1574029ff`;
- 24/24 exact;
- total GM `5.112681x`;
- query GM `5.059393x`;
- zero total/query loss cells;
- **mandatory correction:** all executed workload records have `locality_percent=0`, so PR62 does not establish source-policy breadth.

### PR66 — VeloGraphX NY source reuse

- run `38730045262`;
- artifact `11971060916`;
- ZIP SHA-256 `c4a3d64af948afd03daa1e983ee2d75eadf838ca94bdc59103c25ff5575dd299`;
- 24/24 exact;
- total GM `15.561191x`;
- query GM `3.897866x`;
- zero total/query loss cells;
- hot-pool cache hits `1164`, rotating-hot hits `576`.

### PR80 — VeloGraphX non-road preregistered holdout

- run `38856829047`;
- artifact `12027088809`;
- ZIP SHA-256 `617bb4c9c847b8c236dea95caf39119569d66422038be32f5498502298eef100`;
- 24/24 exact;
- total GM `157.67741734463613x`;
- query GM `30.199378953227633x`;
- zero total/query loss cells;
- three frozen graph seeds × four source regimes × two workload seeds;
- 64 MiB common maximum logical-state allowance;
- interval interpretation remains slice-level robustness, **not population inference**.

### PR88 — independent raw-artifact recomputation

- run `38865585699`;
- retained artifact ID `12030529254`;
- retained artifact digest `sha256:edb03af36c4c6188866c0ec10a86c2394ea95ea8d185e93ee375a224534b1ee5`;
- re-downloaded PR62/PR66/PR80 artifacts by ID;
- verified their ZIP hashes;
- ignored archived aggregate summaries;
- recomputed exact-cell counts, geometric means, log-space t intervals, loss cells, minima, and experiment-specific cache/state/seed facts from raw per-cell files.

This is called **independent recomputation** in the manuscript. It is not described as independent third-party experimental replication.

## Mandatory manuscript boundaries

PR89 must retain all of the following:

1. **No universal/theoretical SOTA claim.**
2. **No population-level inference claim** from the 24-cell intervals.
3. **VeloGraphX arbitrary-source semantics are adapter-mediated**, because pinned upstream is fixed-source.
4. **The 64 MiB contract is a common maximum allowance**, not equal RSS and not equal actual memory occupancy.
5. **PR62 does not establish source-policy breadth.**
6. **Broad algorithmic novelty remains rejected.**
7. **Integrated-system originality remains unestablished.**
8. **Negative evidence remains visible**, including rejected PR57-v1 and the refuted large-subtree rebuild proxy.
9. **PR88 is artifact-level independent recomputation**, not external third-party replication.
10. GraphBolt and NetworKit remain contextual/assumption-separated rather than being silently promoted into the central DIRECT claim.

## Acceptance rule

PR89 passes only if all citation, bibliography, evidence, claim-boundary, historical-scaffold, and diff-surface checks pass simultaneously.

A bibliography or prose repair may not alter solver/comparator/workload/evidence code. If manuscript completion exposes a genuine scientific contradiction, the correct R7 outcome is to fail PR89 and reopen the relevant evidence stage rather than editing the prose around the contradiction.
