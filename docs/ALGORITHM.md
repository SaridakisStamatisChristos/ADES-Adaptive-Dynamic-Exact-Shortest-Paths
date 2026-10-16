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
- a scalar `repair_epoch` diagnostic/lifecycle counter;
- adaptive repair-controller estimates;
- residency statistics: hits, recency, and accumulated update debt.

The `SSSPState` structure retains a compatibility `repair_mark` vector, but newly constructed states do not use it as resident semantic payload. Increase-repair membership marking is implemented with one reusable thread-local epoch-mark workspace shared across synchronous repairs. This scratch is transient execution state rather than per-resident persistent state.

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

Increase-repair subtree discovery does not change semantic shortest-path state (`dist`, selected parent, or SPT links) before the affected region has been accepted within the discovery budget, except for an exact early alternate-witness rescue that changes only the affected root's selected parent after proving the replacement witness is independent of the increased edge. Epoch marks are scratch state and may change during discovery.

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

### 7.2 Tight non-selected-parent filter

If the old edge was tight but was **not** the selected parent of \(v\), the implementation returns `Filtered` without discovery or semantic mutation.

This is exact because the changed edge occurs nowhere in the selected shortest-path witness tree. Every reachable vertex therefore retains its complete selected shortest-path witness after the edge increases. An increase cannot create a shorter path; because an unchanged shortest witness remains for every vertex, no shortest distance can increase either.

This PR54 filter is intentionally based on the existence of the unchanged selected witness tree, not on storing every equal shortest-path predecessor.

### 7.3 Selected-parent edge and read-only discovery

If the increased edge is the selected parent of \(v\), every vertex whose selected-tree path from \(s\) passes through \(v\) forms a candidate invalidated region \(A\): the selected SPT subtree rooted at \(v\).

ADES discovers \(A\) read-only using the persistent child/sibling links and the shared thread-local membership scratch.

During discovery it counts vertices and selected-tree edges. If a configured/adaptive discovery or total-work budget is exceeded, repair aborts **before broad semantic shortest-path mutation** and the source is rebuilt with exact Dijkstra.

### 7.4 Large-repair-gated alternate-witness rescue

The frozen candidate contains the accepted PR57-v2 rescue path. It does **not** probe every selected-parent increase.

- For repairs with fewer than 256 discovered affected vertices, it performs zero witness scans.
- When discovery reaches 256 affected vertices, it performs one bounded scan of at most 64 incoming arcs of the affected root.
- It ignores the increased edge itself and considers only old-distance-tight alternatives.
- If a tight predecessor has strictly smaller distance than the affected root, nonnegative weights imply that predecessor cannot be a descendant of the root in the selected SPT; the root can be safely reparented immediately and the old exact distances remain valid.
- Tight equal-distance candidates are retained but not accepted immediately, because a zero-weight same-distance predecessor may still lie inside the affected subtree. After full read-only discovery, such a candidate is accepted only if its tail is outside \(A\).
- If no safe alternate witness is found, execution continues to the ordinary boundary reconstruction path.

No extra persistent shortest-path DAG is stored. Probe work is bounded and is telemetry-visible.

### 7.5 Boundary reconstruction

If discovery is accepted and no alternate-witness rescue ends the repair:

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
        return FILTERED

    A = empty
    alternate_candidates = empty
    witness_probe_ran = false

    DFS/stack over selected-tree subtree rooted at v
    while discovering:
        add vertex to A
        count affected vertices and selected-tree edges
        if budget exceeded:
            return REBUILD_REQUIRED

        if not witness_probe_ran and |A| >= 256:
            witness_probe_ran = true
            scan at most 64 incoming arcs of v
            for each unchanged tight predecessor p:
                if dist[p] < dist[v]:
                    reparent v to p
                    return REPAIRED
                if dist[p] = dist[v]:
                    retain candidate for post-discovery check

    if witness_probe_ran:
        for each retained equal-distance predecessor p:
            if p is outside A and remains tight:
                reparent v to p
                return REPAIRED

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

### Lemma I2 — tight non-selected-parent filter safety

If the increased tight edge is not the selected parent of its head, it occurs nowhere in the selected witness tree. Every vertex therefore retains an unchanged selected shortest path. Since an edge increase cannot create a shorter path, all distances remain exactly unchanged.

### Lemma I3 — localization for a selected-parent increase

If the increased edge is the selected parent edge into \(v\), vertices outside the selected-tree subtree \(A\) retain at least one selected shortest-path witness that does not use the changed selected-parent edge.

Thus semantic invalidation is confined to \(A\) for the selected-tree representation unless a safe alternate witness proves that even the root retains an unchanged-distance route.

### Lemma I4 — alternate-witness rescue safety

Assume the old increased selected edge enters root \(v\), and another unchanged incoming edge \((p,v)\) remains tight at the old exact distance.

- If `dist[p] < dist[v]`, then with nonnegative weights \(p\) cannot be a descendant of \(v\) along a selected-tree path, because descendant distances cannot be smaller than the ancestor's distance. Reparenting \(v\) to \((p,v)\) therefore preserves an independent exact witness for \(v\) and all unchanged selected descendants.
- If `dist[p] = dist[v]`, the candidate is accepted only after complete subtree discovery proves \(p\notin A\). The candidate then has an exact selected witness independent of the increased edge, so the same reparenting argument applies.

No distance label changes during a successful rescue.

### Lemma I5 — boundary-seeded restricted Dijkstra restores exactness in \(A\)

Assume exact labels outside \(A\). Any path from \(s\) to a vertex in \(A\) enters \(A\) for the first time through some boundary edge \((y,x)\), with \(y\notin A\), \(x\in A\). Therefore the exact distance to every vertex in \(A\) is the shortest path obtained from exact outside labels plus one boundary edge followed by a path entirely inside \(A\). Multi-source Dijkstra initialized by all such boundary candidates computes exactly these values.

### Corollary I6 — early abort is exact

If discovery exceeds budget before semantic invalidation/reconstruction, the implementation performs full Dijkstra from the resident source. The fallback replaces the resident state with a fresh exact SSSP state.

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

where \(P\) is priority-queue pop count. The implementation also records bounded alternate-witness scan/rescue telemetry; PR57 deliberately did not silently fold probe work into historical learned work units when introducing the gated mechanism.

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
- An increase is safely filtered when non-tight by Lemma I1 or when tight but non-selected-parent by Lemma I2; for a selected-parent increase it may be rescued by an independent alternate witness under Lemma I4, exactly repaired by Lemmas I3 and I5 for an accepted affected subtree, or exactly rebuilt after early abort by Corollary I6.
- Eviction, cooldown, admission and controller-budget choices select among exact execution paths and therefore cannot change the semantic answer.

Hence I1 is restored after every update and all queries are exact. ∎

This is a repository-level proof sketch tied to the implemented cases. It is not presented as machine-checked formal verification.

---

## 11. Complexity

Let \(n=|V|\), \(m=|E|\).

The implementation uses monotone/radix priority queues in the frozen candidate's resident build/repair paths where applicable; the bounds below retain standard comparison-heap-style asymptotic notation for a conservative architecture-level description unless otherwise stated.

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

Conservatively,

\[
O((n+m)\log n)
\]

time and \(O(n)\) resident SSSP/SPT state, in addition to graph storage.

### Decrease repair

Let \(R\) be the region whose labels strictly improve and let \(E_R\) denote outgoing edges scanned during propagation. A conservative comparison-heap bound is

\[
O((|R|+|E_R|)\log n),
\]

while the actual frozen candidate uses its monotone/radix queue for this propagation. In the worst case \(R=V\), so repair still degenerates to full-SSSP scale.

### Non-tight or tight non-selected-parent increase

After constant-time label/parent tests, both cases are filtered without subtree discovery:

\[
O(1)
\]

apart from ordinary per-resident update-loop overhead.

### Selected-parent increase discovery

If \(A\) is the discovered selected-tree subtree, discovery costs

\[
O(|A|+|E_{tree}(A)|).
\]

Discovery may terminate earlier at the configured budget.

When discovery first reaches 256 vertices, PR57-v2 may additionally inspect at most 64 incoming arcs of the affected root once, so the alternate-witness probe adds only a fixed bounded scan under the frozen constants.

### Increase accepted repair

Let

- \(B_A\) be incoming boundary edges scanned for vertices in \(A\);
- \(E_A\) be outgoing edges scanned during restricted propagation;
- \(P_A\) be priority-queue operations.

A conservative architecture-level bound is

\[
O(|A|+|E_{tree}(A)|+|B_A|+|E_A|+P_A\log n),
\]

with the frozen implementation using its radix queue for the restricted propagation. In the worst case \(A=V\), so local repair has no universal asymptotic advantage over rebuilding.

### Repair abort

A full rebuild has conservative bound

\[
O((n+m)\log n).
\]

### Update across multiple residents

If \(k\) sources are resident, an edge update is processed independently for each resident. Thus worst-case update cost is the sum of the \(k\) per-source filter/repair/rebuild costs.

### Cache memory

ADES bounds the number of full resident source states by `resident_cap`. The principal algorithmic memory tradeoff is therefore

\[
O(n+m) + O(k n)
\]

plus per-source controller/probation metadata. Under the accounting-v2 representation, the principal per-resident vertex payload is distance plus 32-bit parent/child/sibling identifiers; repair-membership epoch marks are shared transient scratch rather than one persistent array per resident.

---

## 12. What ADES does and does not claim

The implemented architecture supports the following precise characterization:

> ADES is an exact adaptive shortest-path architecture for directed nonnegative weighted graphs with online edge-weight increases and decreases. It dynamically chooses between cold bidirectional search and bounded resident SSSP maintenance, using exact local repairs where justified and exact full rebuild as a conservative fallback.

Current evidence supports adaptive residency, exactness under the tested operation families, strong avoidance of unnecessary resident maintenance, and scoped practical performance-SOTA evidence under the separately frozen publication contract.

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
- decrease/increase repair and gated alternate-witness rescue: `src/dynamic_repair.cpp`;
- repair-cost controller: `include/ades/repair_controller.hpp`;
- source lifecycle, admission, query/update orchestration: `src/ades.cpp`;
- adversarial/differential validation: `tests/`;
- benchmark methodology and evidence: `docs/benchmark-methodology.md`, `docs/BENCHMARK_PROTOCOL.md`, and `evidence/`.

The source code remains authoritative where implementation details and this specification diverge; such divergence should be treated as a documentation defect and repaired explicitly.
