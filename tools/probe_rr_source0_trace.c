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
 * The external source is compiled unchanged.  This harness supplies only the
 * graph/trace adapter and an independent indexed-binary-heap Dijkstra oracle.
 */

#define ORACLE_INF (UINT64_MAX / UINT64_C(4))

typedef struct {
    uint32_t u;
    uint32_t v;
    int w;
    int next;
} edge_record;

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

static int compare_u64(const void *lhs, const void *rhs) {
    const uint64_t a = *(const uint64_t *)lhs;
    const uint64_t b = *(const uint64_t *)rhs;
    return (a > b) - (a < b);
}

static uint64_t pair_key(uint32_t u, uint32_t v) {
    return ((uint64_t)u << 32) | (uint64_t)v;
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

static edge_record *load_dimacs(const char *path, vertex **rr_graph_out,
                                oracle_graph *model, int *parallel_out,
                                int *edge_domain_ok_out) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open DIMACS graph");

    char line[512];
    uint32_t n = 0, declared_m = 0, m = 0;
    edge_record *edges = NULL;
    int *first = NULL;
    vertex *rr_graph = NULL;
    uint64_t *keys = NULL;
    int edge_domain_ok = 1;

    while (fgets(line, sizeof(line), in)) {
        if (line[0] == 'p') {
            unsigned nn = 0, mm = 0;
            if (sscanf(line, "p sp %u %u", &nn, &mm) == 2) {
                n = nn;
                declared_m = mm;
                if (n == 0 || n > (uint32_t)INT_MAX) fail("unsupported vertex count");
                edges = (edge_record *)calloc((size_t)declared_m, sizeof(edge_record));
                keys = (uint64_t *)malloc((size_t)declared_m * sizeof(uint64_t));
                first = (int *)malloc((size_t)n * sizeof(int));
                rr_graph = g_create_graph((int)n);
                if (!edges || !keys || !first || !rr_graph) fail("graph allocation failed");
                for (uint32_t i = 0; i < n; ++i) first[i] = -1;
            }
        } else if (line[0] == 'a') {
            unsigned u1 = 0, v1 = 0;
            unsigned long long w64 = 0;
            if (sscanf(line, "a %u %u %llu", &u1, &v1, &w64) != 3) continue;
            if (!rr_graph || m >= declared_m || u1 == 0 || v1 == 0 || u1 > n || v1 > n)
                fail("invalid DIMACS arc");
            if (w64 > (unsigned long long)(INT_MAX - 500)) edge_domain_ok = 0;
            const int w = w64 > INT_MAX ? INT_MAX : (int)w64;
            const uint32_t u = u1 - 1, v = v1 - 1;
            edges[m] = (edge_record){u, v, w, first[u]};
            first[u] = (int)m;
            keys[m] = pair_key(u, v);
            if (!g_insert_edge(rr_graph, (int)u, (int)v, w)) fail("g_insert_edge failed");
            ++m;
        }
    }
    fclose(in);
    if (!rr_graph || n == 0 || m != declared_m) fail("DIMACS count mismatch");

    qsort(keys, m, sizeof(uint64_t), compare_u64);
    int parallel = 0;
    for (uint32_t i = 1; i < m; ++i) {
        if (keys[i] == keys[i - 1]) {
            parallel = 1;
            break;
        }
    }
    free(keys);

    model->n = n;
    model->m = m;
    model->edges = edges;
    model->first = first;
    *rr_graph_out = rr_graph;
    *parallel_out = parallel;
    *edge_domain_ok_out = edge_domain_ok;
    return edges;
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
                         const oracle_graph *model, int parallel,
                         int edge_domain_ok, int path_domain_ok,
                         int source0_only, uint32_t op_count,
                         uint32_t query_count, uint32_t update_count,
                         uint32_t increase_count, uint32_t decrease_count,
                         uint32_t completed_ops, const compare_result *cmp,
                         int exact) {
    FILE *out = fopen(path, "w");
    if (!out) fail("cannot write report");
    fprintf(out, "{\n");
    fprintf(out, "  \"schema\": \"ades-rr-source0-ny-preflight-v2\",\n");
    fprintf(out, "  \"rr_commit\": \"20aa865a52b67bd0ff402e2946e51541db0b5473\",\n");
    fprintf(out, "  \"graph\": \"%s\",\n", graph_path);
    fprintf(out, "  \"vertices\": %u,\n", model->n);
    fprintf(out, "  \"edges\": %u,\n", model->m);
    fprintf(out, "  \"parallel_arc_ambiguity\": %s,\n", parallel ? "true" : "false");
    fprintf(out, "  \"edge_int_domain_compatible\": %s,\n", edge_domain_ok ? "true" : "false");
    fprintf(out, "  \"path_int_domain_compatible\": %s,\n", path_domain_ok ? "true" : "false");
    fprintf(out, "  \"source0_only\": %s,\n", source0_only ? "true" : "false");
    fprintf(out, "  \"operation_count\": %u,\n", op_count);
    fprintf(out, "  \"query_count\": %u,\n", query_count);
    fprintf(out, "  \"update_count\": %u,\n", update_count);
    fprintf(out, "  \"increase_count\": %u,\n", increase_count);
    fprintf(out, "  \"decrease_count\": %u,\n", decrease_count);
    fprintf(out, "  \"completed_operations\": %u,\n", completed_ops);
    fprintf(out, "  \"full_state_checks\": %u,\n", cmp->full_state_checks);
    fprintf(out, "  \"vertex_distance_comparisons\": %" PRIu64 ",\n", cmp->comparison_count);
    fprintf(out, "  \"mismatch_count\": %" PRIu64 ",\n", cmp->mismatch_count);
    fprintf(out, "  \"exact_full_state\": %s,\n", exact ? "true" : "false");
    fprintf(out, "  \"heap_capacity\": %d,\n", HEAP_SIZE);
    fprintf(out, "  \"oracle\": \"independent indexed-binary-heap Dijkstra in ADES harness\",\n");
    fprintf(out, "  \"weight_change_adapter\": \"rr_remove_edge then rr_add_edge\"");
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
    oracle_graph model = {0};
    int parallel = 0, edge_domain_ok = 0;
    load_dimacs(argv[1], &rr_graph, &model, &parallel, &edge_domain_ok);

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

    if (parallel || !edge_domain_ok || !source0_only) {
        write_report(argv[3], argv[1], &model, parallel, edge_domain_ok,
                     cmp.path_domain_ok, source0_only, op_count, query_count,
                     update_count, increase_count, decrease_count, 0, &cmp, 0);
        fprintf(stderr,
                "[rr-ny] adapter precondition failure parallel=%d edge_domain=%d source0=%d\n",
                parallel, edge_domain_ok, source0_only);
        free(ops);
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

    uint32_t completed = 0;
    int phase = 0;
    for (uint32_t i = 0; i < op_count; ++i) {
        trace_op *op = &ops[i];
        if (op->kind == OP_UPDATE) {
            edge_record *e = &model.edges[op->a];
            if ((uint64_t)e->w != op->old_weight) fail("trace old weight differs from model state");
            rr_remove_edge(rr_graph, (int)e->u, (int)e->v, q);
            rr_add_edge(rr_graph, (int)e->u, (int)e->v, (int)op->new_weight, q);
            e->w = (int)op->new_weight;
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
    write_report(argv[3], argv[1], &model, parallel, edge_domain_ok,
                 cmp.path_domain_ok, source0_only, op_count, query_count,
                 update_count, increase_count, decrease_count, completed,
                 &cmp, exact);
    fprintf(stderr,
            "[rr-ny] vertices=%u edges=%u ops=%u updates=%u full_checks=%u comparisons=%" PRIu64 " mismatches=%" PRIu64 " exact=%d heap_capacity=%d\n",
            model.n, model.m, op_count, update_count, cmp.full_state_checks,
            cmp.comparison_count, cmp.mismatch_count, exact, HEAP_SIZE);

    free_queue(q);
    g_free_graph(&rr_graph, (int)model.n);
    free(oracle_positions);
    free(oracle_heap_nodes);
    free(oracle_dist);
    free(ops);
    free(model.first);
    free(model.edges);
    return 0;
}
