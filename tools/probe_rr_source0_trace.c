#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "heap.h"
#include "rr.h"

/*
 * NY-scale source-0 qualification harness for the pinned third-party RR code.
 * The external source is compiled unchanged.
 *
 * ADES/DIMACS may contain parallel directed arcs.  The RR public removal API
 * identifies an arc only by (tail,head), so the adapter uses the exact SSSP
 * reduction of every parallel bundle to one arc with weight=min(bundle).
 * When an original edge changes, the RR aggregate arc is updated iff that
 * bundle minimum changes.  The independent oracle always keeps the full
 * original multigraph, so any error in this reduction is exposed directly.
 */

#define ORACLE_INF (UINT64_MAX / UINT64_C(4))

typedef struct {
    uint32_t u;
    uint32_t v;
    int w;
    int next;
} edge_record;

typedef struct {
    uint32_t u;
    uint32_t v;
    int min_w;
} group_record;

typedef struct {
    uint64_t key;
    uint32_t edge_id;
} sort_item;

typedef enum { OP_QUERY = 1, OP_UPDATE = 2 } op_kind;

typedef struct {
    op_kind kind;
    uint32_t a;
    uint32_t b;
    uint64_t old_weight;
    uint64_t new_weight;
} trace_op;

typedef struct {
    uint32_t *nodes;
    int *position;
    uint64_t *distance;
    uint32_t size;
} oracle_heap;

typedef struct {
    uint32_t n;
    uint32_t m;
    edge_record *edges;
    int *first;
    uint32_t *group_of_edge;
    group_record *groups;
    uint32_t group_count;
    uint32_t parallel_extra_edges;
} oracle_graph;

typedef struct {
    uint64_t mismatch_count;
    uint64_t comparison_count;
    uint32_t full_state_checks;
    int first_bad_phase;
    uint32_t first_bad_node;
    int first_rr;
    uint64_t first_oracle;
    int path_domain_ok;
} compare_result;

static void fail(const char *message) {
    fprintf(stderr, "probe_rr_source0_trace: %s\n", message);
    exit(2);
}

static uint64_t pair_key(uint32_t u, uint32_t v) {
    return ((uint64_t)u << 32) | (uint64_t)v;
}

static int compare_sort_item(const void *lhs, const void *rhs) {
    const sort_item *a = (const sort_item *)lhs;
    const sort_item *b = (const sort_item *)rhs;
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return (a->edge_id > b->edge_id) - (a->edge_id < b->edge_id);
}

static int rr_distance(vertex *g, uint32_t node) {
    return ((vtx_node *)g[node].h_node.key)->cost;
}

static void free_queue(heap *q) {
    if (!q) return;
    free(q->node_vector);
    free(q);
}

static void heap_swap(oracle_heap *h, uint32_t i, uint32_t j) {
    const uint32_t a = h->nodes[i], b = h->nodes[j];
    h->nodes[i] = b;
    h->nodes[j] = a;
    h->position[a] = (int)j;
    h->position[b] = (int)i;
}

static void heap_sift_up(oracle_heap *h, uint32_t i) {
    while (i > 0) {
        const uint32_t parent = (i - 1) / 2;
        if (h->distance[h->nodes[parent]] <= h->distance[h->nodes[i]]) break;
        heap_swap(h, parent, i);
        i = parent;
    }
}

static void heap_sift_down(oracle_heap *h, uint32_t i) {
    for (;;) {
        uint32_t best = i;
        const uint32_t left = 2 * i + 1, right = left + 1;
        if (left < h->size &&
            h->distance[h->nodes[left]] < h->distance[h->nodes[best]]) best = left;
        if (right < h->size &&
            h->distance[h->nodes[right]] < h->distance[h->nodes[best]]) best = right;
        if (best == i) break;
        heap_swap(h, i, best);
        i = best;
    }
}

static void heap_push_or_decrease(oracle_heap *h, uint32_t v) {
    const int pos = h->position[v];
    if (pos < 0) {
        const uint32_t i = h->size++;
        h->nodes[i] = v;
        h->position[v] = (int)i;
        heap_sift_up(h, i);
    } else {
        heap_sift_up(h, (uint32_t)pos);
    }
}

static uint32_t heap_pop(oracle_heap *h) {
    if (h->size == 0) fail("oracle heap underflow");
    const uint32_t root = h->nodes[0];
    h->position[root] = -2;
    --h->size;
    if (h->size) {
        h->nodes[0] = h->nodes[h->size];
        h->position[h->nodes[0]] = 0;
        heap_sift_down(h, 0);
    }
    return root;
}

static void oracle_dijkstra(const oracle_graph *g, uint64_t *dist,
                            uint32_t *heap_nodes, int *positions) {
    for (uint32_t v = 0; v < g->n; ++v) {
        dist[v] = ORACLE_INF;
        positions[v] = -1;
    }
    oracle_heap h = {heap_nodes, positions, dist, 0};
    dist[0] = 0;
    heap_push_or_decrease(&h, 0);

    while (h.size) {
        const uint32_t u = heap_pop(&h);
        for (int id = g->first[u]; id >= 0; id = g->edges[id].next) {
            const edge_record *e = &g->edges[id];
            const uint64_t candidate = dist[u] + (uint64_t)e->w;
            if (candidate < dist[e->v]) {
                dist[e->v] = candidate;
                if (positions[e->v] == -2) fail("oracle Dijkstra settled-node decrease");
                heap_push_or_decrease(&h, e->v);
            }
        }
    }
}

static void compare_full_state(vertex *rr_graph, const oracle_graph *model,
                               uint64_t *dist, uint32_t *heap_nodes,
                               int *positions, compare_result *result,
                               int phase) {
    oracle_dijkstra(model, dist, heap_nodes, positions);
    ++result->full_state_checks;
    for (uint32_t v = 0; v < model->n; ++v) {
        ++result->comparison_count;
        const int got = rr_distance(rr_graph, v);
        const uint64_t want = dist[v];
        int matches = 0;
        if (want == ORACLE_INF) {
            matches = got >= INF;
        } else if (want >= (uint64_t)INF) {
            result->path_domain_ok = 0;
            matches = 0;
        } else {
            matches = got == (int)want;
        }
        if (!matches) {
            if (result->mismatch_count == 0) {
                result->first_bad_phase = phase;
                result->first_bad_node = v;
                result->first_rr = got;
                result->first_oracle = want;
            }
            ++result->mismatch_count;
        }
    }
}

static int recompute_group_min(const oracle_graph *model, uint32_t group_id) {
    int min_w = INT_MAX;
    for (uint32_t id = 0; id < model->m; ++id) {
        if (model->group_of_edge[id] == group_id && model->edges[id].w < min_w)
            min_w = model->edges[id].w;
    }
    if (min_w == INT_MAX) fail("empty parallel group");
    return min_w;
}

static void load_dimacs(const char *path, vertex **rr_graph_out,
                        oracle_graph *model, int *edge_domain_ok_out) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open DIMACS graph");

    char line[512];
    uint32_t n = 0, declared_m = 0, m = 0;
    edge_record *edges = NULL;
    int *first = NULL;
    sort_item *sorted = NULL;
    int edge_domain_ok = 1;

    while (fgets(line, sizeof(line), in)) {
        if (line[0] == 'p') {
            unsigned nn = 0, mm = 0;
            if (sscanf(line, "p sp %u %u", &nn, &mm) == 2) {
                n = nn;
                declared_m = mm;
                if (n == 0 || n > (uint32_t)INT_MAX) fail("unsupported vertex count");
                edges = (edge_record *)calloc((size_t)declared_m, sizeof(edge_record));
                sorted = (sort_item *)malloc((size_t)declared_m * sizeof(sort_item));
                first = (int *)malloc((size_t)n * sizeof(int));
                if (!edges || !sorted || !first) fail("graph allocation failed");
                for (uint32_t i = 0; i < n; ++i) first[i] = -1;
            }
        } else if (line[0] == 'a') {
            unsigned u1 = 0, v1 = 0;
            unsigned long long w64 = 0;
            if (sscanf(line, "a %u %u %llu", &u1, &v1, &w64) != 3) continue;
            if (!edges || m >= declared_m || u1 == 0 || v1 == 0 || u1 > n || v1 > n)
                fail("invalid DIMACS arc");
            if (w64 > (unsigned long long)(INT_MAX - 500)) edge_domain_ok = 0;
            const int w = w64 > INT_MAX ? INT_MAX : (int)w64;
            const uint32_t u = u1 - 1, v = v1 - 1;
            edges[m] = (edge_record){u, v, w, first[u]};
            first[u] = (int)m;
            sorted[m] = (sort_item){pair_key(u, v), m};
            ++m;
        }
    }
    fclose(in);
    if (!edges || n == 0 || m != declared_m) fail("DIMACS count mismatch");

    qsort(sorted, m, sizeof(sort_item), compare_sort_item);
    uint32_t *group_of_edge = (uint32_t *)malloc((size_t)m * sizeof(uint32_t));
    group_record *groups = (group_record *)calloc((size_t)m, sizeof(group_record));
    if (!group_of_edge || !groups) fail("parallel-group allocation failed");

    uint32_t group_count = 0, parallel_extra = 0;
    for (uint32_t i = 0; i < m;) {
        const uint64_t key = sorted[i].key;
        const uint32_t start = i;
        int min_w = INT_MAX;
        while (i < m && sorted[i].key == key) {
            const uint32_t edge_id = sorted[i].edge_id;
            group_of_edge[edge_id] = group_count;
            if (edges[edge_id].w < min_w) min_w = edges[edge_id].w;
            ++i;
        }
        const uint32_t width = i - start;
        if (width > 1) parallel_extra += width - 1;
        groups[group_count] = (group_record){
            (uint32_t)(key >> 32), (uint32_t)key, min_w};
        ++group_count;
    }
    free(sorted);

    vertex *rr_graph = g_create_graph((int)n);
    if (!rr_graph) fail("g_create_graph failed");
    for (uint32_t gid = 0; gid < group_count; ++gid) {
        const group_record *group = &groups[gid];
        if (!g_insert_edge(rr_graph, (int)group->u, (int)group->v, group->min_w))
            fail("RR aggregate edge insertion failed");
    }

    model->n = n;
    model->m = m;
    model->edges = edges;
    model->first = first;
    model->group_of_edge = group_of_edge;
    model->groups = groups;
    model->group_count = group_count;
    model->parallel_extra_edges = parallel_extra;
    *rr_graph_out = rr_graph;
    *edge_domain_ok_out = edge_domain_ok;
}

static trace_op *read_trace(const char *path, uint32_t *count_out,
                            uint32_t *queries_out, uint32_t *updates_out,
                            uint32_t *increases_out, uint32_t *decreases_out) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open trace");
    uint32_t capacity = 256, count = 0, queries = 0, updates = 0;
    uint32_t increases = 0, decreases = 0;
    trace_op *ops = (trace_op *)calloc(capacity, sizeof(trace_op));
    if (!ops) fail("trace allocation failed");
    char line[512];
    while (fgets(line, sizeof(line), in)) {
        if (count == capacity) {
            capacity *= 2;
            trace_op *grown = (trace_op *)realloc(ops, (size_t)capacity * sizeof(trace_op));
            if (!grown) fail("trace realloc failed");
            ops = grown;
        }
        unsigned a = 0, b = 0;
        unsigned long long oldw = 0, neww = 0;
        if (sscanf(line, "QUERY %u %u", &a, &b) == 2) {
            ops[count++] = (trace_op){OP_QUERY, a, b, 0, 0};
            ++queries;
        } else if (sscanf(line, "UPDATE %u %llu %llu", &a, &oldw, &neww) == 3) {
            ops[count++] = (trace_op){OP_UPDATE, a, 0, oldw, neww};
            ++updates;
            if (neww > oldw) ++increases;
            else if (neww < oldw) ++decreases;
        } else if (line[0] != '\0' && line[0] != '\n' && line[0] != '#') {
            fail("invalid trace line");
        }
    }
    fclose(in);
    *count_out = count;
    *queries_out = queries;
    *updates_out = updates;
    *increases_out = increases;
    *decreases_out = decreases;
    return ops;
}

static void write_report(const char *path, const char *graph_path,
                         const oracle_graph *model, int edge_domain_ok,
                         int path_domain_ok, int source0_only,
                         uint32_t op_count, uint32_t query_count,
                         uint32_t update_count, uint32_t increase_count,
                         uint32_t decrease_count, uint32_t completed_ops,
                         uint32_t aggregate_updates, uint32_t suppressed_updates,
                         const compare_result *cmp, int exact) {
    FILE *out = fopen(path, "w");
    if (!out) fail("cannot write report");
    fprintf(out, "{\n");
    fprintf(out, "  \"schema\": \"ades-rr-source0-ny-preflight-v3\",\n");
    fprintf(out, "  \"rr_commit\": \"20aa865a52b67bd0ff402e2946e51541db0b5473\",\n");
    fprintf(out, "  \"graph\": \"%s\",\n", graph_path);
    fprintf(out, "  \"vertices\": %u,\n", model->n);
    fprintf(out, "  \"original_edges\": %u,\n", model->m);
    fprintf(out, "  \"rr_aggregate_edges\": %u,\n", model->group_count);
    fprintf(out, "  \"parallel_extra_edges\": %u,\n", model->parallel_extra_edges);
    fprintf(out, "  \"parallel_edges_present\": %s,\n", model->parallel_extra_edges ? "true" : "false");
    fprintf(out, "  \"parallel_min_collapse_adapter\": true,\n");
    fprintf(out, "  \"edge_int_domain_compatible\": %s,\n", edge_domain_ok ? "true" : "false");
    fprintf(out, "  \"path_int_domain_compatible\": %s,\n", path_domain_ok ? "true" : "false");
    fprintf(out, "  \"source0_only\": %s,\n", source0_only ? "true" : "false");
    fprintf(out, "  \"operation_count\": %u,\n", op_count);
    fprintf(out, "  \"query_count\": %u,\n", query_count);
    fprintf(out, "  \"update_count\": %u,\n", update_count);
    fprintf(out, "  \"increase_count\": %u,\n", increase_count);
    fprintf(out, "  \"decrease_count\": %u,\n", decrease_count);
    fprintf(out, "  \"aggregate_rr_updates\": %u,\n", aggregate_updates);
    fprintf(out, "  \"underlying_updates_suppressed\": %u,\n", suppressed_updates);
    fprintf(out, "  \"completed_operations\": %u,\n", completed_ops);
    fprintf(out, "  \"full_state_checks\": %u,\n", cmp->full_state_checks);
    fprintf(out, "  \"vertex_distance_comparisons\": %" PRIu64 ",\n", cmp->comparison_count);
    fprintf(out, "  \"mismatch_count\": %" PRIu64 ",\n", cmp->mismatch_count);
    fprintf(out, "  \"exact_full_state\": %s,\n", exact ? "true" : "false");
    fprintf(out, "  \"heap_capacity\": %d,\n", HEAP_SIZE);
    fprintf(out, "  \"oracle\": \"independent indexed-binary-heap Dijkstra on original multigraph\",\n");
    fprintf(out, "  \"weight_change_adapter\": \"update original edge; recompute bundle minimum; rr_remove_edge+rr_add_edge only if minimum changes\"");
    if (cmp->first_bad_phase >= 0) {
        fprintf(out,
                ",\n  \"first_mismatch\": {\"phase\": %d, \"node\": %u, \"rr\": %d, \"oracle\": %" PRIu64 "}",
                cmp->first_bad_phase, cmp->first_bad_node, cmp->first_rr,
                cmp->first_oracle);
    }
    fprintf(out, "\n}\n");
    fclose(out);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s GRAPH.gr TRACE.txt REPORT.json\n", argv[0]);
        return 2;
    }

    vertex *rr_graph = NULL;
    oracle_graph model;
    memset(&model, 0, sizeof(model));
    int edge_domain_ok = 0;
    load_dimacs(argv[1], &rr_graph, &model, &edge_domain_ok);

    uint32_t op_count = 0, query_count = 0, update_count = 0;
    uint32_t increase_count = 0, decrease_count = 0;
    trace_op *ops = read_trace(argv[2], &op_count, &query_count, &update_count,
                               &increase_count, &decrease_count);

    int source0_only = 1;
    for (uint32_t i = 0; i < op_count; ++i) {
        if (ops[i].kind == OP_QUERY && ops[i].a != 0) source0_only = 0;
        if (ops[i].kind == OP_UPDATE) {
            if (ops[i].a >= model.m ||
                ops[i].new_weight > (uint64_t)(INT_MAX - 500)) edge_domain_ok = 0;
        }
    }

    compare_result cmp;
    memset(&cmp, 0, sizeof(cmp));
    cmp.first_bad_phase = -1;
    cmp.path_domain_ok = 1;

    if (!edge_domain_ok || !source0_only) {
        write_report(argv[3], argv[1], &model, edge_domain_ok,
                     cmp.path_domain_ok, source0_only, op_count, query_count,
                     update_count, increase_count, decrease_count, 0, 0, 0,
                     &cmp, 0);
        fprintf(stderr, "[rr-ny] adapter precondition failure edge_domain=%d source0=%d\n",
                edge_domain_ok, source0_only);
        free(ops);
        free(model.groups);
        free(model.group_of_edge);
        free(model.first);
        free(model.edges);
        g_free_graph(&rr_graph, (int)model.n);
        return 0;
    }

    uint64_t *oracle_dist = (uint64_t *)malloc((size_t)model.n * sizeof(uint64_t));
    uint32_t *oracle_heap_nodes = (uint32_t *)malloc((size_t)model.n * sizeof(uint32_t));
    int *oracle_positions = (int *)malloc((size_t)model.n * sizeof(int));
    if (!oracle_dist || !oracle_heap_nodes || !oracle_positions) fail("oracle workspace allocation failed");

    heap *q = heap_new();
    if (!q) fail("heap_new failed");
    ((vtx_node *)rr_graph[0].h_node.key)->cost = 0;
    rr_graph[0].pi = -2;
    if (heap_insert(&rr_graph[0].h_node, q) != 0) fail("source heap insert failed");
    rr_recalculate_shortest_path(rr_graph, q);

    compare_full_state(rr_graph, &model, oracle_dist, oracle_heap_nodes,
                       oracle_positions, &cmp, -1);

    uint32_t completed = 0, aggregate_updates = 0, suppressed_updates = 0;
    int phase = 0;
    for (uint32_t i = 0; i < op_count; ++i) {
        trace_op *op = &ops[i];
        if (op->kind == OP_UPDATE) {
            edge_record *edge = &model.edges[op->a];
            if ((uint64_t)edge->w != op->old_weight)
                fail("trace old weight differs from original-edge state");
            const uint32_t gid = model.group_of_edge[op->a];
            group_record *group = &model.groups[gid];
            const int old_min = group->min_w;
            edge->w = (int)op->new_weight;
            const int new_min = recompute_group_min(&model, gid);
            if (new_min != old_min) {
                rr_remove_edge(rr_graph, (int)group->u, (int)group->v, q);
                rr_add_edge(rr_graph, (int)group->u, (int)group->v, new_min, q);
                group->min_w = new_min;
                ++aggregate_updates;
            } else {
                ++suppressed_updates;
            }
            compare_full_state(rr_graph, &model, oracle_dist, oracle_heap_nodes,
                               oracle_positions, &cmp, phase++);
        } else {
            const uint64_t want = oracle_dist[op->b];
            const int got = rr_distance(rr_graph, op->b);
            int matches;
            if (want == ORACLE_INF) matches = got >= INF;
            else if (want >= (uint64_t)INF) {
                cmp.path_domain_ok = 0;
                matches = 0;
            } else matches = got == (int)want;
            if (!matches) {
                if (cmp.mismatch_count == 0) {
                    cmp.first_bad_phase = phase;
                    cmp.first_bad_node = op->b;
                    cmp.first_rr = got;
                    cmp.first_oracle = want;
                }
                ++cmp.mismatch_count;
                ++cmp.comparison_count;
            }
        }
        ++completed;
    }

    const int exact = cmp.mismatch_count == 0 && cmp.path_domain_ok;
    write_report(argv[3], argv[1], &model, edge_domain_ok,
                 cmp.path_domain_ok, source0_only, op_count, query_count,
                 update_count, increase_count, decrease_count, completed,
                 aggregate_updates, suppressed_updates, &cmp, exact);
    fprintf(stderr,
            "[rr-ny] vertices=%u original_edges=%u aggregate_edges=%u parallel_extra=%u ops=%u updates=%u aggregate_updates=%u full_checks=%u comparisons=%" PRIu64 " mismatches=%" PRIu64 " exact=%d heap_capacity=%d\n",
            model.n, model.m, model.group_count, model.parallel_extra_edges,
            op_count, update_count, aggregate_updates, cmp.full_state_checks,
            cmp.comparison_count, cmp.mismatch_count, exact, HEAP_SIZE);

    free_queue(q);
    g_free_graph(&rr_graph, (int)model.n);
    free(oracle_positions);
    free(oracle_heap_nodes);
    free(oracle_dist);
    free(ops);
    free(model.groups);
    free(model.group_of_edge);
    free(model.first);
    free(model.edges);
    return 0;
}
