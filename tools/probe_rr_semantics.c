#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "heap.h"
#include "rr.h"

/*
 * Falsification-first semantic probe for the pinned third-party
 * felipe31/Ramalingam-Reps implementation.
 *
 * The external algorithm is not modified.  This harness only:
 *   1. builds a directed graph through its public graph helpers;
 *   2. initializes the supplied source-0 SSSP state;
 *   3. adapts one ADES weight change as rr_remove_edge + rr_add_edge;
 *   4. checks every maintained source-0 distance against an independent
 *      Bellman-Ford model after every operation.
 */

#define MODEL_INF ((int64_t)INT_MAX * 4LL)
#define MAX_EDGES 256
#define MAX_UPDATES 160
#define MAX_MISMATCHES 16

struct model_edge {
    int u;
    int v;
    int w;
};

struct update {
    int u;
    int v;
    int w;
};

struct mismatch {
    int step;
    int node;
    int rr_distance;
    int64_t oracle_distance;
};

struct case_state {
    const char *name;
    int n;
    struct model_edge edges[MAX_EDGES];
    int edge_count;
    struct update updates[MAX_UPDATES];
    int update_count;
};

struct result {
    const char *name;
    int pass;
    int completed_steps;
    int increases;
    int decreases;
    int noops;
    int mismatch_count;
    struct mismatch mismatches[MAX_MISMATCHES];
};

static void die(const char *message) {
    fprintf(stderr, "probe_rr_semantics: %s\n", message);
    exit(2);
}

static void add_model_edge(struct case_state *c, int u, int v, int w) {
    if (c->edge_count >= MAX_EDGES) die("too many model edges");
    c->edges[c->edge_count++] = (struct model_edge){u, v, w};
}

static void add_update(struct case_state *c, int u, int v, int w) {
    if (c->update_count >= MAX_UPDATES) die("too many updates");
    c->updates[c->update_count++] = (struct update){u, v, w};
}

static int find_model_edge(const struct case_state *c, int u, int v) {
    int found = -1;
    for (int i = 0; i < c->edge_count; ++i) {
        if (c->edges[i].u == u && c->edges[i].v == v) {
            if (found != -1) die("parallel edges are not supported by this adapter probe");
            found = i;
        }
    }
    return found;
}

static void bellman_ford(const struct case_state *c, int64_t *dist) {
    for (int i = 0; i < c->n; ++i) dist[i] = MODEL_INF;
    dist[0] = 0;
    for (int pass = 1; pass < c->n; ++pass) {
        int changed = 0;
        for (int i = 0; i < c->edge_count; ++i) {
            const struct model_edge *e = &c->edges[i];
            if (dist[e->u] == MODEL_INF) continue;
            const int64_t candidate = dist[e->u] + (int64_t)e->w;
            if (candidate < dist[e->v]) {
                dist[e->v] = candidate;
                changed = 1;
            }
        }
        if (!changed) break;
    }
}

static int normalized_rr_distance(vertex *g, int node) {
    const int value = ((vtx_node *)g[node].h_node.key)->cost;
    return value >= INF ? INF : value;
}

static int distance_matches(int rr_distance, int64_t oracle_distance) {
    if (oracle_distance >= MODEL_INF / 2) return rr_distance >= INF;
    if (oracle_distance >= INF) return 0;
    return rr_distance == (int)oracle_distance;
}

static void record_comparison(struct result *r, vertex *g,
                              const struct case_state *c, int step) {
    int64_t dist[128];
    if (c->n > (int)(sizeof(dist) / sizeof(dist[0]))) die("case too large");
    bellman_ford(c, dist);
    for (int v = 0; v < c->n; ++v) {
        const int got = normalized_rr_distance(g, v);
        if (!distance_matches(got, dist[v])) {
            r->pass = 0;
            if (r->mismatch_count < MAX_MISMATCHES) {
                r->mismatches[r->mismatch_count] =
                    (struct mismatch){step, v, got, dist[v]};
            }
            ++r->mismatch_count;
        }
    }
}

static vertex *init_rr(const struct case_state *c, heap **queue_out) {
    vertex *g = g_create_graph(c->n);
    if (!g) die("g_create_graph failed");
    for (int i = 0; i < c->edge_count; ++i) {
        const struct model_edge *e = &c->edges[i];
        if (!g_insert_edge(g, e->u, e->v, e->w)) die("g_insert_edge failed");
    }

    heap *q = heap_new();
    if (!q) die("heap_new failed");
    ((vtx_node *)g[0].h_node.key)->cost = 0;
    g[0].pi = -2;
    if (heap_insert(&g[0].h_node, q) != 0) die("source heap insertion failed");
    rr_recalculate_shortest_path(g, q);
    *queue_out = q;
    return g;
}

static void free_rr(vertex **g, int n, heap *q) {
    if (g && *g) g_free_graph(g, n);
    if (q) {
        free(q->node_vector);
        free(q);
    }
}

static struct result run_case(struct case_state *c) {
    struct result r;
    memset(&r, 0, sizeof(r));
    r.name = c->name;
    r.pass = 1;

    heap *q = NULL;
    vertex *g = init_rr(c, &q);
    fprintf(stderr, "[%s] initialized source-0 SSSP\n", c->name);
    record_comparison(&r, g, c, -1);

    for (int step = 0; step < c->update_count; ++step) {
        const struct update u = c->updates[step];
        const int index = find_model_edge(c, u.u, u.v);
        if (index < 0) die("update edge missing from model");
        const int old = c->edges[index].w;
        if (u.w > old) ++r.increases;
        else if (u.w < old) ++r.decreases;
        else ++r.noops;

        fprintf(stderr,
                "[%s] step=%d edge=(%d,%d) old=%d new=%d begin\n",
                c->name, step, u.u, u.v, old, u.w);

        /* Deliberately leave the external RR algorithm unchanged. */
        rr_remove_edge(g, u.u, u.v, q);
        rr_add_edge(g, u.u, u.v, u.w, q);
        c->edges[index].w = u.w;

        fprintf(stderr, "[%s] step=%d remove+add complete\n", c->name, step);
        record_comparison(&r, g, c, step);
        ++r.completed_steps;
    }

    free_rr(&g, c->n, q);
    return r;
}

static struct case_state make_simple(void) {
    struct case_state c;
    memset(&c, 0, sizeof(c));
    c.name = "simple-directed-increase-decrease";
    c.n = 5;
    const int e[][3] = {
        {0,1,1},{1,2,1},{0,2,5},{2,3,1},{1,3,10},{3,4,1},{0,4,50}
    };
    for (size_t i = 0; i < sizeof(e)/sizeof(e[0]); ++i) add_model_edge(&c,e[i][0],e[i][1],e[i][2]);
    add_update(&c,1,2,10);
    add_update(&c,0,2,1);
    add_update(&c,2,3,20);
    add_update(&c,1,3,1);
    return c;
}

static struct case_state make_zero(void) {
    struct case_state c;
    memset(&c, 0, sizeof(c));
    c.name = "zero-weight-equal-distance";
    c.n = 7;
    const int e[][3] = {
        {0,1,0},{0,2,0},{1,3,0},{2,3,0},{3,4,1},{1,5,2},{2,5,2},{5,6,1}
    };
    for (size_t i = 0; i < sizeof(e)/sizeof(e[0]); ++i) add_model_edge(&c,e[i][0],e[i][1],e[i][2]);
    add_update(&c,1,3,1);
    add_update(&c,2,3,2);
    add_update(&c,1,3,0);
    add_update(&c,0,2,3);
    return c;
}

static struct case_state make_asymmetric(void) {
    struct case_state c;
    memset(&c, 0, sizeof(c));
    c.name = "asymmetric-directed-arcs";
    c.n = 6;
    const int e[][3] = {
        {0,1,1},{1,0,30},{1,2,1},{2,1,40},{0,3,8},{3,2,1},
        {2,4,1},{4,2,50},{4,5,1},{1,5,20}
    };
    for (size_t i = 0; i < sizeof(e)/sizeof(e[0]); ++i) add_model_edge(&c,e[i][0],e[i][1],e[i][2]);
    add_update(&c,1,2,15);
    add_update(&c,0,3,2);
    add_update(&c,3,2,20);
    add_update(&c,1,5,2);
    return c;
}

static uint64_t next_rng(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * UINT64_C(2685821657736338717);
}

static struct case_state make_random(void) {
    struct case_state c;
    memset(&c, 0, sizeof(c));
    c.name = "random-directed-mixed";
    c.n = 32;
    uint64_t rng = UINT64_C(20261015);

    for (int u = 0; u < c.n; ++u) {
        const int v = (u + 1) % c.n;
        add_model_edge(&c, u, v, 1 + (int)(next_rng(&rng) % 5));
    }
    while (c.edge_count < 180) {
        const int u = (int)(next_rng(&rng) % (uint64_t)c.n);
        const int v = (int)(next_rng(&rng) % (uint64_t)c.n);
        if (u == v || find_model_edge(&c, u, v) >= 0) continue;
        add_model_edge(&c, u, v, (int)(next_rng(&rng) % 16));
    }

    for (int step = 0; step < 120; ++step) {
        const int idx = (int)(next_rng(&rng) % (uint64_t)c.edge_count);
        const int old = c.edges[idx].w;
        int nw;
        if ((step % 2) == 0 || old == 0) {
            nw = old + 1 + (int)(next_rng(&rng) % 10);
        } else {
            const int max_delta = old < 8 ? old : 8;
            nw = old - (1 + (int)(next_rng(&rng) % (uint64_t)max_delta));
        }
        add_update(&c, c.edges[idx].u, c.edges[idx].v, nw);
    }
    return c;
}

static void write_result(const char *path, const struct result *r,
                         const char *commit) {
    FILE *out = fopen(path, "w");
    if (!out) die("cannot open report path");
    fprintf(out, "{\n");
    fprintf(out, "  \"schema\": \"ades-rr-semantic-case-v1\",\n");
    fprintf(out, "  \"rr_commit\": \"%s\",\n", commit);
    fprintf(out, "  \"case\": \"%s\",\n", r->name);
    fprintf(out, "  \"adapter\": \"weight_change_as_remove_then_add\",\n");
    fprintf(out, "  \"source\": 0,\n");
    fprintf(out, "  \"pass\": %s,\n", r->pass ? "true" : "false");
    fprintf(out, "  \"completed_steps\": %d,\n", r->completed_steps);
    fprintf(out, "  \"increase_updates\": %d,\n", r->increases);
    fprintf(out, "  \"decrease_updates\": %d,\n", r->decreases);
    fprintf(out, "  \"noop_updates\": %d,\n", r->noops);
    fprintf(out, "  \"mismatch_count\": %d,\n", r->mismatch_count);
    fprintf(out, "  \"mismatches\": [\n");
    const int stored = r->mismatch_count < MAX_MISMATCHES ? r->mismatch_count : MAX_MISMATCHES;
    for (int i = 0; i < stored; ++i) {
        const struct mismatch *m = &r->mismatches[i];
        fprintf(out,
                "    {\"step\": %d, \"node\": %d, \"rr\": %d, \"oracle\": %lld}%s\n",
                m->step, m->node, m->rr_distance,
                (long long)m->oracle_distance, i + 1 == stored ? "" : ",");
    }
    fprintf(out, "  ]\n}\n");
    if (fclose(out) != 0) die("failed to close report");
}

static struct case_state make_named(const char *name) {
    if (strcmp(name, "simple") == 0) return make_simple();
    if (strcmp(name, "zero") == 0) return make_zero();
    if (strcmp(name, "asymmetric") == 0) return make_asymmetric();
    if (strcmp(name, "random") == 0) return make_random();
    die("unknown --case value");
    return make_simple();
}

int main(int argc, char **argv) {
    if (argc != 5 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case {simple|zero|asymmetric|random} REPORT.json RR_COMMIT\n", argv[0]);
        return 2;
    }
    struct case_state c = make_named(argv[2]);
    struct result r = run_case(&c);
    write_result(argv[3], &r, argv[4]);
    fprintf(stderr, "[%s] %s steps=%d mismatches=%d\n",
            r.name, r.pass ? "PASS" : "MISMATCH", r.completed_steps,
            r.mismatch_count);
    /* Semantic mismatch is evidence, not harness failure. */
    return 0;
}
