# ADES — Formal Algorithm Specification

This document specifies the implemented **Adaptive Dynamic Exact Shortest Paths (ADES)** algorithm independently of the C++ source. It defines the online problem, state, operations, exactness invariants, local-repair rules, adaptive residency policy, correctness obligations, and complexity bounds implemented by the repository.

ADES is an **exact** online shortest-path architecture. Its adaptive mechanisms decide *how much state to maintain* and *when to repair or rebuild*; they do not relax correctness. Novelty and universal asymptotic superiority are not claimed here.

---

## 1. Online problem

Let

\[
G=(V,E,w)
\]

be a directed graph with fixed vertex/edge topology and nonnegative integer edge weights

\[
w:E\to \mathbb{Z}_{\ge 0}.
\]

The operation stream is arbitrary and online. ADES receives no future operations.

Two operations are supported:

- `QUERY(s,t)`: return the exact shortest-path distance \(\delta_G(s,t)\), or infinity when \(t\) is unreachable from \(s\).
- `UPDATE(e,w')`: replace the weight of an existing stable edge \(e\) by nonnegative integer \(w'\). Both increases and decreases are permitted.

The graph topology does not change. Edge identifiers remain stable across updates.

Distances use saturating addition so arithmetic overflow is represented as infinity rather than wrapping.

---

## 2. State

### 2.1 Graph state

ADES stores:

- immutable edge endpoints;
- mutable nonnegative edge weights;
- stable edge identifiers;
- forward adjacency;
- reverse adjacency.

### 2.2 Source lifecycle

A source may be in one of three conceptual states:

1. **Cold** — no maintained single-source shortest-path state.
2. **Probation** — still cold, but recent cold-query work for this source is accumulated to decide whether residency is economically justified.
3. **Resident** — a full exact SSSP state is maintained for the source.

The implementation stores cold and probation sources outside the resident table; probation is represented by accumulated query count and cold edge scans.

### 2.3 Resident SSSP state

For each resident source \(s\), ADES stores:

- `dist[v]`: exact \(\delta_G(s,v)\);
- `parent_edge[v]`: one selected shortest-path incoming edge for reachable \(v\ne s\), or no parent;
- intrusive `first_child`, `next_sibling`, `prev_sibling` links encoding the selected shortest-path tree (SPT);
- epoch-mark scratch space used to identify an affected SPT region during increase repair;
- adaptive repair-controller estimates;
- residency statistics: hits, recency, and accumulated update debt.

The selected parent is not intended to encode every shortest-path predecessor. It is a single witness tree used to localize certain invalidations.

---

## 3. Exactness invariants

After every completed operation, every resident source \(s\) must satisfy:

### I1. Exact distance invariant

\[
\forall v\in V:\quad dist_s[v]=\delta_G(s,v).
\]

### I2. Parent witness invariant

For every reachable \(v\ne s\) with selected parent edge \(e=(u,v)\),

\[
dist_s[v]=dist_s[u]+w(e).
\]

Unreachable vertices have no selected parent.

### I3. Selected-tree consistency

The intrusive child/sibling representation is consistent with `parent_edge`. Parent changes detach the vertex from its previous parent and attach it to the new parent.

### I4. Strict-improvement parent rule

Ordinary Dijkstra relaxation and decrease repair change a parent only when a strictly shorter distance is found. Equal-distance alternatives do not trigger reparenting.

This avoids cycles induced purely by zero-weight/equal-distance reparenting.

### I5. Discovery-before-mutation rule

Increase-repair subtree discovery does not change semantic shortest-path state (`dist`, selected parent, or SPT links) before the affected region has been accepted within the discovery budget. Epoch marks are scratch state and may change during discovery.

If discovery aborts, exact semantic state is discarded through a full exact rebuild rather than partially repaired.

### I6. Policy independence

Promotion, eviction, cooldown, admission hysteresis, and repair-budget selection may affect execution cost but must not change the returned shortest-path value. Every permitted fallback is an exact shortest-path computation.

---

## 4. Cold query algorithm

For a nonresident source, ADES executes exact bidirectional Dijkstra using forward search from \(s\) and reverse search from \(t\).

```text
COLD-QUERY(s, t)
    if s = t:
        return 0

    initialize forward distance df[s] = 0
    initialize backward distance db[t] = 0
    best = infinity

    while both priority queues are nonempty:
        if min_forward_key + min_backward_key >= best:
            break

        expand the side with smaller minimum key
        relax its outgoing edges (forward) or incoming edges (backward)
        whenever the two searches have compatible labels,
            improve best

    return best
```

The implementation also records scanned-edge work. This work is used only by the promotion policy.

For a resident source, `QUERY(s,t)` returns the maintained exact `dist[t]` in constant lookup time.

---

## 5. Promotion and residency

For each cold/probation source \(s\), ADES accumulates:

- number of cold queries \(q_s\);
- total cold bidirectional edge scans \(X_s\).

Let \(m=|E|\). The current admission trigger is

\[
q_s \ge q_{min}
\quad\text{and}\quad
X_s \ge \rho m,
\]

where `probation_queries = q_min` and `promotion_ratio = rho` are configuration parameters.

This deliberately uses observed cold-search work rather than recurrence alone: a frequently queried source that is cheap to answer cold need not become resident.

When capacity is available, promotion performs a full Dijkstra SSSP build.

When the resident cache is full, each resident entry \(r\) receives the score

\[
score(r)=
\frac{hits(r)}
{1+age(r)+\lambda\,updateDebt(r)}.
\]

The weakest resident is the candidate victim. A new source is admitted only if its candidate score exceeds the victim score by the configured hysteresis factor. An evicted source enters a cooldown interval before it may be admitted again.

This policy is heuristic and performance-oriented. It is not part of the exactness proof.

---

## 6. Weight-decrease repair

Suppose edge \(e=(u,v)\) changes from \(w\) to \(w'<w\).

### Safe filter

If \(u\) was unreachable from resident source \(s\), the update cannot create a path through \(u\) and is filtered for this resident source.

Otherwise compute

\[
cand = dist_s[u]+w'.
\]

If

\[
cand \ge dist_s[v],
\]

then the decreased edge cannot improve \(v\), and therefore cannot improve any path whose only newly improved prefix would pass through \(v\). The resident state is unchanged.

### Relevant decrease

If

\[
cand < dist_s[v],
\]

set

\[
dist_s[v]\leftarrow cand
\]

and select \(e\) as the parent of \(v\). Run a Dijkstra-style propagation seeded only at \(v\), relaxing strictly improving outgoing edges until no label improves.

```text
REPAIR-DECREASE(e=(u,v), w')
    if dist[u] = infinity:
        return FILTERED

    cand = dist[u] + w'
    if cand >= dist[v]:
        return FILTERED

    dist[v] = cand
    parent[v] = e
    PQ = {(cand, v)}

    while PQ not empty:
        (du, x) = extract-min(PQ)
        if du != dist[x]:
            continue
        for each edge (x,y):
            nd = du + w(x,y)
            if nd < dist[y]:
                dist[y] = nd
                parent[y] = (x,y)
                push (nd,y)

    return REPAIRED
```

### Lemma D1 — decrease filter safety

If `dist[u] + w' >= dist[v]`, no shortest distance can decrease because of the update.

**Argument.** Any newly improved path using the changed edge contains a prefix from \(s\) to \(u\), then the changed edge, giving cost at least `dist[u] + w'`. Since this does not improve \(v\), continuing from \(v\) cannot produce a path whose suffix could not already be appended to an equally good or better old prefix to \(v\).

### Lemma D2 — seeded decrease propagation restores exactness

When the changed edge improves \(v\), Dijkstra propagation initialized with all unchanged exact labels plus the improved label at \(v\) restores exact distances.

**Reason.** Only distances reachable through a newly improved prefix can decrease; all edge weights remain nonnegative, and standard Dijkstra relaxation from the newly improved frontier finds exactly those improvements.

---

## 7. Weight-increase repair

Suppose edge \(e=(u,v)\) changes from \(w\) to \(w'>w\). The decision is based on the **old** edge and old exact labels.

### 7.1 Non-tight filter

If

\[
dist_s[u]+w \ne dist_s[v],
\]

then the old edge was not tight and could not be a shortest-path witness into \(v\). Increasing it cannot invalidate any shortest-path distance. The update is filtered.

### 7.2 Tight non-selected-parent edge

If the old edge was tight but was not the selected parent of \(v\), the current implementation conservatively requests a full rebuild.

This is intentional. A single selected SPT does not encode all equal-length shortest-path alternatives, so the implementation does not claim that a tight non-parent increase is locally ignorable.

### 7.3 Selected-parent edge

If the increased edge is the selected parent of \(v\), every vertex whose selected-tree path from \(s\) passes through \(v\) forms a candidate invalidated region \(A\): the selected SPT subtree rooted at \(v\).

ADES discovers \(A\) read-only using the persistent child/sibling links.

During discovery it counts vertices and selected-tree edges. If either configured/adaptive discovery budget is exceeded, repair aborts **before semantic shortest-path mutation** and the source is rebuilt with exact Dijkstra.

### 7.4 Boundary reconstruction

If discovery is accepted:

1. set `dist[x]=infinity` and remove selected parents for all \(x\in A\);
2. scan incoming edges \((y,x)\) for each \(x\in A\) where \(y\notin A\);
3. seed \(x\) with the best finite boundary candidate
   \[
   dist[y]+w(y,x);
   \]
4. run Dijkstra restricted to edges whose head is inside \(A\).

```text
REPAIR-INCREASE(old e=(u,v), new weight, budget)
    if dist[u] = infinity or dist[u] + old_weight != dist[v]:
        return FILTERED

    if parent[v] != e:
        return REBUILD_REQUIRED

    A = empty
    DFS/stack over selected-tree subtree rooted at v
    while discovering:
        count affected vertices and selected-tree edges
        if budget exceeded:
            return REBUILD_REQUIRED

    for x in A:
        dist[x] = infinity
        parent[x] = none

    PQ = empty
    for x in A:
        for each incoming edge (y,x):
            if y not in A and dist[y] finite:
                relax x from dist[y] + w(y,x)
        if dist[x] finite:
            push x

    while PQ not empty:
        (dx,x) = extract-min(PQ)
        if dx != dist[x]:
            continue
        for each outgoing edge (x,y):
            if y in A:
                relax y normally

    return REPAIRED
```

### Lemma I1 — non-tight increase filter safety

If the old edge was not tight under exact old labels, increasing it cannot invalidate an old shortest path.

**Argument.** Any path using that edge reached \(v\) with cost strictly larger than the known shortest distance to \(v\). Making that edge more expensive cannot turn such a path into a shortest one or invalidate another shortest witness.

### Lemma I2 — localization for a selected-parent increase

If the increased edge is the selected parent edge into \(v\), vertices outside the selected-tree subtree \(A\) retain at least one selected shortest-path witness that does not use the changed selected-parent edge.

Thus semantic invalidation is confined to \(A\) for the selected-tree representation.

### Lemma I3 — boundary-seeded restricted Dijkstra restores exactness in \(A\)

Assume exact labels outside \(A\). Any path from \(s\) to a vertex in \(A\) enters \(A\) for the first time through some boundary edge \((y,x)\), with \(y\notin A\), \(x\in A\). Therefore the exact distance to every vertex in \(A\) is the shortest path obtained from exact outside labels plus one boundary edge followed by a path entirely inside \(A\). Multi-source Dijkstra initialized by all such boundary candidates computes exactly these values.

### Corollary I4 — early abort is exact

If discovery exceeds budget, the implementation performs full Dijkstra from the resident source. Since semantic SSSP state has not yet been partially mutated, the fallback simply replaces the resident state with a fresh exact SSSP state.

---

## 8. Repair controller

The repair controller is a **performance policy**, not a correctness mechanism.

For each resident source it maintains exponentially weighted moving averages (EWMA) of:

- full rebuild time \(\hat C_B\);
- repair nanoseconds per measured work unit \(\hat c_W\);
- repair nanoseconds per discovered vertex \(\hat c_V\);
- work units per discovered vertex;
- selected-tree edges per discovered vertex.

Measured repair work is

\[
W = V_A + E_{tree} + E_{boundary} + E_{restricted} + P,
\]

where \(P\) is priority-queue pop count.

For the work-aware policy, the target work budget is currently estimated as

\[
W_{target}=\gamma\frac{\hat C_B}{\hat c_W}.
\]

This is translated into discovery vertex/tree-edge limits using the learned work-per-vertex ratios and capped by hard safety ceilings.

The vertex-only policy analogously uses

\[
V_{target}=\gamma\frac{\hat C_B}{\hat c_V}.
\]

A fixed policy uses only the configured hard discovery limits.

### Important empirical limitation

The current controller successfully identifies large affected selected-tree regions, but the archived NY controller experiment shows that abort-and-rebuild can be slower than completing a large local repair. Consequently the hypothesis

> large affected subtree ⇒ rebuilding is economically preferable

is **refuted as a general policy rule** by current evidence.

A stronger future controller should estimate **remaining local-repair cost versus rebuild cost**, rather than treating structural affected-region magnitude as a sufficient economic proxy.

This empirical refutation does not affect exactness.

---

## 9. Top-level ADES operations

```text
QUERY(s,t)
    advance logical query clock

    if s is resident:
        update resident hit/recency statistics
        return resident.dist[t]

    result, scanned_work = exact bidirectional Dijkstra(s,t)
    update probation statistics for s

    if probation trigger is satisfied:
        attempt cost-aware admission
        if admitted:
            build exact resident SSSP by Dijkstra

    return result
```

```text
UPDATE(edge e, new_weight)
    old = current edge
    if old.weight = new_weight:
        return

    install new graph weight

    for each resident source state R:
        increment update debt

        if new_weight < old.weight:
            outcome = REPAIR-DECREASE
        else:
            budget = controller policy capped by hard safety ceiling
            outcome = REPAIR-INCREASE

        if outcome = FILTERED:
            continue

        if outcome = REPAIRED:
            update repair/controller measurements as applicable
            continue

        if outcome = REBUILD_REQUIRED:
            replace R by exact Dijkstra SSSP
            update rebuild measurement
```

---

## 10. Exactness theorem

### Theorem — ADES returns exact shortest-path distances

Assume:

1. graph weights remain nonnegative;
2. saturating arithmetic correctly represents values beyond the supported finite range as infinity;
3. full Dijkstra and bidirectional Dijkstra implementations are exact;
4. resident state initially satisfies invariants I1–I5.

Then after any finite online sequence of supported weight updates and shortest-path queries, every `QUERY(s,t)` returned by ADES equals \(\delta_G(s,t)\).

### Proof sketch

Induct over operations.

- A cold query is answered by exact bidirectional Dijkstra and does not alter graph semantics.
- A resident query returns `dist[t]`, exact by I1.
- A promotion constructs resident state using full exact Dijkstra.
- A decrease is either safely filtered by Lemma D1 or repaired exactly by Lemma D2.
- An increase is either safely filtered by Lemma I1; conservatively rebuilt when a tight non-selected-parent case is encountered; exactly repaired by Lemmas I2–I3 for an accepted selected-parent subtree; or exactly rebuilt after early abort by Corollary I4.
- Eviction, cooldown, admission and controller-budget choices select among exact execution paths and therefore cannot change the semantic answer.

Hence I1 is restored after every update and all queries are exact. ∎

This is a repository-level proof sketch tied to the implemented cases. It is not presented as machine-checked formal verification.

---

## 11. Complexity

Let \(n=|V|\), \(m=|E|\).

With binary heaps:

### Cold query

Worst case:

\[
O((n+m)\log n)
\]

time and \(O(n)\) transient distance/search state, although bidirectional search may inspect much less of the graph in practice.

### Resident query

\[
O(1)
\]

time for a distance lookup once a source is resident.

### Resident build/rebuild

\[
O((n+m)\log n)
\]

time and \(O(n)\) resident SSSP/SPT state, in addition to graph storage.

### Decrease repair

Let \(R\) be the region whose labels strictly improve and let \(E_R\) denote outgoing edges scanned during propagation. The repair cost is output-sensitive:

\[
O((|R|+|E_R|)\log n)
\]

up to heap constants. In the worst case \(R=V\), so the bound degenerates to full-SSSP scale.

### Increase discovery

If \(A\) is the accepted selected-tree subtree, discovery costs

\[
O(|A|+|E_{tree}(A)|).
\]

Discovery may terminate earlier at the configured budget.

### Increase accepted repair

Let

- \(B_A\) be incoming boundary edges scanned for vertices in \(A\);
- \(E_A\) be outgoing edges scanned during restricted Dijkstra;
- \(P_A\) be priority-queue operations.

Then repair is bounded by

\[
O(|A|+|E_{tree}(A)|+|B_A|+|E_A|+P_A\log n).
\]

In the worst case \(A=V\), so local repair has no universal asymptotic advantage over rebuilding.

### Tight non-parent increase or repair abort

A full rebuild costs

\[
O((n+m)\log n).
\]

### Update across multiple residents

If \(k\) sources are resident, an edge update is processed independently for each resident. Thus worst-case update cost is the sum of the \(k\) per-source repair/rebuild costs.

### Cache memory

ADES bounds the number of full resident source states by `resident_cap`. The principal algorithmic memory tradeoff is therefore

\[
O(n+m) + O(k n)
\]

plus per-source controller/probation metadata, with implementation constants determined by distance, parent, SPT-link and repair-mark arrays.

---

## 12. What ADES does and does not claim

The implemented architecture supports the following precise characterization:

> ADES is an exact adaptive shortest-path architecture for directed nonnegative weighted graphs with online edge-weight increases and decreases. It dynamically chooses between cold bidirectional search and bounded resident SSSP maintenance, using exact local repairs where justified and exact full rebuild as a conservative fallback.

Current evidence supports adaptive residency, exactness under the tested operation families, strong avoidance of unnecessary resident maintenance, and substantial memory savings relative to unconditional residency.

This specification does **not** claim:

- a new worst-case asymptotic bound for fully dynamic exact shortest paths;
- universal superiority over bidirectional Dijkstra;
- universal superiority of local repair over rebuild;
- publication-level novelty until a dedicated literature preflight establishes it;
- machine-checked formal verification.

---

## 13. Implementation correspondence

The principal implementation mapping is:

- graph and saturating arithmetic: `include/ades/graph.hpp`, `src/graph.cpp`;
- Dijkstra, bidirectional Dijkstra, parent/SPT maintenance: `src/shortest_paths.cpp`;
- decrease/increase repair: `src/dynamic_repair.cpp`;
- repair-cost controller: `include/ades/repair_controller.hpp`;
- source lifecycle, admission, query/update orchestration: `src/ades.cpp`;
- adversarial/differential validation: `tests/`;
- benchmark methodology and evidence: `docs/benchmark-methodology.md`, `docs/BENCHMARK_PROTOCOL.md`, and `evidence/`.

The source code remains authoritative where implementation details and this specification diverge; such divergence should be treated as a documentation defect and repaired explicitly.
