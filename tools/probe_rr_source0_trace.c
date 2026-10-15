#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "heap.h"
#include "rr.h"

/* NY-scale source-0 trace adapter for the pinned third-party RR code.
 * The external source remains unchanged. */

typedef struct {
    int u;
    int v;
    int w;
} edge_record;

typedef enum { OP_QUERY = 1, OP_UPDATE = 2 } op_kind;

typedef struct {
    op_kind kind;
    uint32_t a;
    uint32_t b;
    uint64_t old_weight;
    uint64_t new_weight;
    uint64_t expected;
} trace_op;

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

static edge_record *load_dimacs(const char *path, vertex **graph_out,
                                uint32_t *n_out, uint32_t *m_out,
                                int *parallel_out, int *domain_ok_out) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open DIMACS graph");

    char line[512];
    uint32_t n = 0, declared_m = 0, m = 0;
    edge_record *edges = NULL;
    vertex *g = NULL;
    uint64_t *keys = NULL;
    int domain_ok = 1;

    while (fgets(line, sizeof(line), in)) {
        if (line[0] == 'p') {
            unsigned nn = 0, mm = 0;
            if (sscanf(line, "p sp %u %u", &nn, &mm) == 2) {
                n = nn;
                declared_m = mm;
                edges = (edge_record *)calloc((size_t)declared_m, sizeof(edge_record));
                keys = (uint64_t *)malloc((size_t)declared_m * sizeof(uint64_t));
                g = g_create_graph((int)n);
                if (!edges || !keys || !g) fail("graph allocation failed");
            }
        } else if (line[0] == 'a') {
            unsigned u1 = 0, v1 = 0;
            unsigned long long w64 = 0;
            if (sscanf(line, "a %u %u %llu", &u1, &v1, &w64) != 3) continue;
            if (!g || m >= declared_m || u1 == 0 || v1 == 0 || u1 > n || v1 > n)
                fail("invalid DIMACS arc");
            if (w64 > (unsigned long long)(INT_MAX - 500)) domain_ok = 0;
            const int w = w64 > INT_MAX ? INT_MAX : (int)w64;
            const uint32_t u = u1 - 1, v = v1 - 1;
            edges[m] = (edge_record){(int)u, (int)v, w};
            keys[m] = pair_key(u, v);
            if (!g_insert_edge(g, (int)u, (int)v, w)) fail("g_insert_edge failed");
            ++m;
        }
    }
    fclose(in);
    if (!g || n == 0 || m != declared_m) fail("DIMACS count mismatch");

    qsort(keys, m, sizeof(uint64_t), compare_u64);
    int parallel = 0;
    for (uint32_t i = 1; i < m; ++i) {
        if (keys[i] == keys[i - 1]) {
            parallel = 1;
            break;
        }
    }
    free(keys);

    *graph_out = g;
    *n_out = n;
    *m_out = m;
    *parallel_out = parallel;
    *domain_ok_out = domain_ok;
    return edges;
}

static trace_op *read_trace(const char *path, uint32_t *count_out,
                            uint32_t *queries_out, uint32_t *updates_out) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open trace");
    uint32_t capacity = 256, count = 0, queries = 0, updates = 0;
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
            ops[count++] = (trace_op){OP_QUERY, a, b, 0, 0, 0};
            ++queries;
        } else if (sscanf(line, "UPDATE %u %llu %llu", &a, &oldw, &neww) == 3) {
            ops[count++] = (trace_op){OP_UPDATE, a, 0, oldw, neww, 0};
            ++updates;
        } else if (line[0] != '\0' && line[0] != '\n' && line[0] != '#') {
            fail("invalid trace line");
        }
    }
    fclose(in);
    *count_out = count;
    *queries_out = queries;
    *updates_out = updates;
    return ops;
}

static void bind_oracle(const char *path, trace_op *ops, uint32_t op_count,
                        uint32_t query_count) {
    FILE *in = fopen(path, "r");
    if (!in) fail("cannot open oracle");
    char magic[64], sha[128];
    unsigned long long declared_ops = 0, declared_queries = 0;
    if (fscanf(in, "%63s %llu %llu %127s", magic, &declared_ops,
               &declared_queries, sha) != 4 || strcmp(magic, "ADES_ORACLE_V2") != 0)
        fail("bad oracle header");
    if (declared_ops != op_count || declared_queries != query_count)
        fail("oracle count mismatch");

    for (uint32_t i = 0; i < op_count; ++i) {
        char kind[32];
        if (fscanf(in, "%31s", kind) != 1) fail("truncated oracle");
        if (ops[i].kind == OP_QUERY) {
            unsigned s = 0, t = 0;
            unsigned long long answer = 0;
            if (strcmp(kind, "QUERY") != 0 ||
                fscanf(in, "%u %u %llu", &s, &t, &answer) != 3 ||
                s != ops[i].a || t != ops[i].b)
                fail("oracle QUERY differs from trace");
            ops[i].expected = answer;
        } else {
            unsigned edge = 0;
            unsigned long long oldw = 0, neww = 0;
            if (strcmp(kind, "UPDATE") != 0 ||
                fscanf(in, "%u %llu %llu", &edge, &oldw, &neww) != 3 ||
                edge != ops[i].a || oldw != ops[i].old_weight ||
                neww != ops[i].new_weight)
                fail("oracle UPDATE differs from trace");
        }
    }
    fclose(in);
}

static void write_report(const char *path, const char *graph_path,
                         uint32_t n, uint32_t m, int parallel, int domain_ok,
                         uint32_t op_count, uint32_t query_count, uint32_t update_count,
                         uint32_t completed_ops, uint32_t mismatches,
                         int first_bad_op, uint32_t first_bad_target,
                         int first_got, uint64_t first_expected,
                         int exact, int source0_only) {
    FILE *out = fopen(path, "w");
    if (!out) fail("cannot write report");
    fprintf(out, "{\n");
    fprintf(out, "  \"schema\": \"ades-rr-source0-ny-preflight-v1\",\n");
    fprintf(out, "  \"rr_commit\": \"20aa865a52b67bd0ff402e2946e51541db0b5473\",\n");
    fprintf(out, "  \"graph\": \"%s\",\n", graph_path);
    fprintf(out, "  \"vertices\": %u,\n", n);
    fprintf(out, "  \"edges\": %u,\n", m);
    fprintf(out, "  \"parallel_arc_ambiguity\": %s,\n", parallel ? "true" : "false");
    fprintf(out, "  \"int_weight_domain_compatible\": %s,\n", domain_ok ? "true" : "false");
    fprintf(out, "  \"source0_only\": %s,\n", source0_only ? "true" : "false");
    fprintf(out, "  \"operation_count\": %u,\n", op_count);
    fprintf(out, "  \"query_count\": %u,\n", query_count);
    fprintf(out, "  \"update_count\": %u,\n", update_count);
    fprintf(out, "  \"completed_operations\": %u,\n", completed_ops);
    fprintf(out, "  \"mismatch_count\": %u,\n", mismatches);
    fprintf(out, "  \"exact_answers\": %s,\n", exact ? "true" : "false");
    fprintf(out, "  \"heap_capacity\": %d,\n", HEAP_SIZE);
    fprintf(out, "  \"weight_change_adapter\": \"rr_remove_edge then rr_add_edge\"");
    if (first_bad_op >= 0) {
        fprintf(out, ",\n  \"first_mismatch\": {\"operation_index\": %d, \"target\": %u, \"rr\": %d, \"oracle\": %" PRIu64 "}",
                first_bad_op, first_bad_target, first_got, first_expected);
    }
    fprintf(out, "\n}\n");
    fclose(out);
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s GRAPH.gr TRACE.txt ORACLE.txt REPORT.json\n", argv[0]);
        return 2;
    }

    vertex *g = NULL;
    uint32_t n = 0, m = 0;
    int parallel = 0, domain_ok = 0;
    edge_record *edges = load_dimacs(argv[1], &g, &n, &m, &parallel, &domain_ok);

    uint32_t op_count = 0, query_count = 0, update_count = 0;
    trace_op *ops = read_trace(argv[2], &op_count, &query_count, &update_count);
    bind_oracle(argv[3], ops, op_count, query_count);

    int source0_only = 1;
    for (uint32_t i = 0; i < op_count; ++i) {
        if (ops[i].kind == OP_QUERY && ops[i].a != 0) source0_only = 0;
        if (ops[i].kind == OP_UPDATE) {
            if (ops[i].a >= m || ops[i].new_weight > (uint64_t)(INT_MAX - 500)) domain_ok = 0;
        }
    }

    if (parallel || !domain_ok || !source0_only) {
        write_report(argv[4], argv[1], n, m, parallel, domain_ok, op_count,
                     query_count, update_count, 0, 0, -1, 0, 0, 0, 0,
                     source0_only);
        fprintf(stderr, "[rr-ny] adapter precondition failure parallel=%d domain_ok=%d source0=%d\n",
                parallel, domain_ok, source0_only);
        free(ops);
        free(edges);
        g_free_graph(&g, (int)n);
        return 0;
    }

    heap *q = heap_new();
    if (!q) fail("heap_new failed");
    ((vtx_node *)g[0].h_node.key)->cost = 0;
    g[0].pi = -2;
    if (heap_insert(&g[0].h_node, q) != 0) fail("source heap insert failed");
    rr_recalculate_shortest_path(g, q);

    uint32_t completed = 0, mismatches = 0;
    int first_bad_op = -1, first_got = 0;
    uint32_t first_bad_target = 0;
    uint64_t first_expected = 0;

    for (uint32_t i = 0; i < op_count; ++i) {
        trace_op *op = &ops[i];
        if (op->kind == OP_UPDATE) {
            edge_record *e = &edges[op->a];
            if ((uint64_t)e->w != op->old_weight) fail("trace old weight differs from adapter state");
            rr_remove_edge(g, e->u, e->v, q);
            rr_add_edge(g, e->u, e->v, (int)op->new_weight, q);
            e->w = (int)op->new_weight;
        } else {
            const int got = rr_distance(g, op->b);
            const uint64_t normalized = got >= INF ? UINT64_MAX : (uint64_t)got;
            const uint64_t expected = op->expected;
            const int expected_inf = expected > UINT64_C(1000000000000000000);
            const int matches = expected_inf ? (got >= INF) : (normalized == expected);
            if (!matches) {
                ++mismatches;
                if (first_bad_op < 0) {
                    first_bad_op = (int)i;
                    first_bad_target = op->b;
                    first_got = got;
                    first_expected = expected;
                }
            }
        }
        ++completed;
    }

    const int exact = mismatches == 0;
    write_report(argv[4], argv[1], n, m, parallel, domain_ok, op_count,
                 query_count, update_count, completed, mismatches,
                 first_bad_op, first_bad_target, first_got, first_expected,
                 exact, source0_only);
    fprintf(stderr,
            "[rr-ny] vertices=%u edges=%u ops=%u completed=%u mismatches=%u exact=%d heap_capacity=%d\n",
            n, m, op_count, completed, mismatches, exact, HEAP_SIZE);

    free_queue(q);
    g_free_graph(&g, (int)n);
    free(edges);
    free(ops);
    return 0;
}
