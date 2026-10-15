#include "graph.h"
#include "heap.h"
#include "rr.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXV 64
#define MAXE 256
#define MODEL_INF ((int64_t)1 << 60)

typedef struct {
  int tail;
  int head;
  int weight;
  int active;
} model_edge;

typedef struct {
  int n;
  int source;
  int edge_count;
  model_edge edges[MAXE];
  vertex *graph;
  heap *queue;
} fixture;

static int rr_distance(const fixture *f, int v) {
  return ((vtx_node *)f->graph[v].h_node.key)->cost;
}

static void model_distances(const fixture *f, int64_t *dist) {
  for (int i = 0; i < f->n; ++i) dist[i] = MODEL_INF;
  dist[f->source] = 0;
  for (int pass = 0; pass < f->n - 1; ++pass) {
    int changed = 0;
    for (int i = 0; i < f->edge_count; ++i) {
      const model_edge *e = &f->edges[i];
      if (!e->active || dist[e->tail] == MODEL_INF) continue;
      const int64_t cand = dist[e->tail] + e->weight;
      if (cand < dist[e->head]) {
        dist[e->head] = cand;
        changed = 1;
      }
    }
    if (!changed) break;
  }
}

static int verify(const fixture *f, const char *case_name, int step) {
  int64_t expected[MAXV];
  model_distances(f, expected);
  for (int v = 0; v < f->n; ++v) {
    const int got = rr_distance(f, v);
    if (expected[v] == MODEL_INF) {
      if (got < INF) {
        fprintf(stderr,
                "MISMATCH case=%s step=%d vertex=%d got=%d expected=INF\n",
                case_name, step, v, got);
        return 0;
      }
    } else if (expected[v] > INT_MAX || got != (int)expected[v]) {
      fprintf(stderr,
              "MISMATCH case=%s step=%d vertex=%d got=%d expected=%lld\n",
              case_name, step, v, got, (long long)expected[v]);
      return 0;
    }
  }
  return 1;
}

static void fixture_init(fixture *f, int n, int source) {
  memset(f, 0, sizeof(*f));
  f->n = n;
  f->source = source;
  f->graph = g_create_graph(n);
  f->queue = heap_new();
  if (!f->graph || !f->queue) {
    fprintf(stderr, "allocation failure\n");
    exit(2);
  }
}

static int add_model_edge(fixture *f, int tail, int head, int weight) {
  if (f->edge_count >= MAXE) {
    fprintf(stderr, "too many model edges\n");
    exit(2);
  }
  const int id = f->edge_count++;
  f->edges[id] = (model_edge){tail, head, weight, 1};
  if (!g_insert_edge(f->graph, tail, head, weight)) {
    fprintf(stderr, "upstream edge insertion failure\n");
    exit(2);
  }
  return id;
}

static void fixture_start(fixture *f) {
  ((vtx_node *)f->graph[f->source].h_node.key)->cost = 0;
  f->graph[f->source].pi = -2;
  if (heap_insert(&f->graph[f->source].h_node, f->queue) != 0) {
    fprintf(stderr, "source heap insertion failure\n");
    exit(2);
  }
  rr_recalculate_shortest_path(f->graph, f->queue);
}

static void fixture_destroy(fixture *f) {
  g_free_graph(&f->graph, f->n);
  free(f->queue);
  f->queue = NULL;
}

/* The only weight-update adapter permitted by the public upstream API:
 * remove the existing directed endpoint pair, then add the new weighted arc.
 * The upstream algorithm itself is not modified. */
static void update_edge(fixture *f, int edge_id, int new_weight) {
  model_edge *e = &f->edges[edge_id];
  rr_remove_edge(f->graph, e->tail, e->head, f->queue);
  rr_add_edge(f->graph, e->tail, e->head, new_weight, f->queue);
  e->weight = new_weight;
}

static int case_simple(void) {
  fixture f;
  fixture_init(&f, 4, 0);
  add_model_edge(&f, 0, 1, 1);
  int e12 = add_model_edge(&f, 1, 2, 1);
  add_model_edge(&f, 0, 2, 5);
  add_model_edge(&f, 2, 3, 2);
  add_model_edge(&f, 1, 3, 10);
  fixture_start(&f);
  if (!verify(&f, "simple", 0)) return fixture_destroy(&f), 0;
  update_edge(&f, e12, 10);
  if (!verify(&f, "simple", 1)) return fixture_destroy(&f), 0;
  update_edge(&f, e12, 0);
  if (!verify(&f, "simple", 2)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

static int case_zero_equal(void) {
  fixture f;
  fixture_init(&f, 5, 0);
  add_model_edge(&f, 0, 1, 0);
  add_model_edge(&f, 0, 2, 0);
  int e13 = add_model_edge(&f, 1, 3, 1);
  int e23 = add_model_edge(&f, 2, 3, 1);
  add_model_edge(&f, 3, 4, 0);
  fixture_start(&f);
  if (!verify(&f, "zero_equal", 0)) return fixture_destroy(&f), 0;
  update_edge(&f, e13, 2);
  if (!verify(&f, "zero_equal", 1)) return fixture_destroy(&f), 0;
  update_edge(&f, e23, 3);
  if (!verify(&f, "zero_equal", 2)) return fixture_destroy(&f), 0;
  update_edge(&f, e13, 1);
  if (!verify(&f, "zero_equal", 3)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

static int case_asymmetric(void) {
  fixture f;
  fixture_init(&f, 4, 0);
  add_model_edge(&f, 0, 1, 2);
  int e10 = add_model_edge(&f, 1, 0, 50);
  int e12 = add_model_edge(&f, 1, 2, 3);
  add_model_edge(&f, 0, 2, 20);
  add_model_edge(&f, 2, 3, 4);
  fixture_start(&f);
  if (!verify(&f, "asymmetric", 0)) return fixture_destroy(&f), 0;
  update_edge(&f, e10, 1);
  if (!verify(&f, "asymmetric", 1)) return fixture_destroy(&f), 0;
  update_edge(&f, e12, 30);
  if (!verify(&f, "asymmetric", 2)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

static int case_nonzero_source(void) {
  fixture f;
  fixture_init(&f, 6, 3);
  add_model_edge(&f, 3, 4, 2);
  add_model_edge(&f, 4, 5, 3);
  add_model_edge(&f, 3, 5, 10);
  add_model_edge(&f, 0, 1, 1);
  fixture_start(&f);
  if (!verify(&f, "nonzero_source", 0)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

static int case_large_cost(void) {
  fixture f;
  fixture_init(&f, 6, 0);
  add_model_edge(&f, 0, 1, 256);
  add_model_edge(&f, 0, 2, 1);
  add_model_edge(&f, 2, 3, 1);
  add_model_edge(&f, 1, 3, 1);
  add_model_edge(&f, 3, 4, 511);
  add_model_edge(&f, 2, 4, 1024);
  int e45 = add_model_edge(&f, 4, 5, 257);
  add_model_edge(&f, 0, 5, 4096);
  fixture_start(&f);
  if (!verify(&f, "large_cost", 0)) return fixture_destroy(&f), 0;
  update_edge(&f, e45, 3);
  if (!verify(&f, "large_cost", 1)) return fixture_destroy(&f), 0;
  update_edge(&f, e45, 513);
  if (!verify(&f, "large_cost", 2)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

static uint32_t rng_state = 0xC0FFEEu;
static uint32_t next_rng(void) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state;
}

static int case_random_mixed(void) {
  fixture f;
  fixture_init(&f, 12, 0);
  int ids[32];
  int count = 0;
  for (int i = 0; i < 11; ++i) ids[count++] = add_model_edge(&f, i, i + 1, 1 + (i % 5));
  ids[count++] = add_model_edge(&f, 0, 5, 8);
  ids[count++] = add_model_edge(&f, 1, 7, 7);
  ids[count++] = add_model_edge(&f, 2, 9, 12);
  ids[count++] = add_model_edge(&f, 3, 10, 4);
  ids[count++] = add_model_edge(&f, 4, 11, 9);
  ids[count++] = add_model_edge(&f, 5, 2, 6);
  ids[count++] = add_model_edge(&f, 7, 4, 5);
  ids[count++] = add_model_edge(&f, 10, 6, 3);
  fixture_start(&f);
  if (!verify(&f, "random_mixed", 0)) return fixture_destroy(&f), 0;
  for (int step = 1; step <= 120; ++step) {
    const int id = ids[next_rng() % (uint32_t)count];
    const int new_weight = (int)(next_rng() % 21u);
    update_edge(&f, id, new_weight);
    if (!verify(&f, "random_mixed", step)) return fixture_destroy(&f), 0;
  }
  fixture_destroy(&f);
  return 1;
}

static int case_parallel_identity(void) {
  fixture f;
  fixture_init(&f, 3, 0);
  /* Insert target edge first, then a second parallel edge. g_insert_edge pushes
   * at the adjacency-list head, while rr_remove_edge searches only by head
   * vertex. The adapter asks to update the first edge by identity. */
  int target = add_model_edge(&f, 0, 1, 1);
  add_model_edge(&f, 0, 1, 5);
  add_model_edge(&f, 1, 2, 1);
  fixture_start(&f);
  if (!verify(&f, "parallel_identity", 0)) return fixture_destroy(&f), 0;
  update_edge(&f, target, 10);
  if (!verify(&f, "parallel_identity", 1)) return fixture_destroy(&f), 0;
  fixture_destroy(&f);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr,
            "usage: rr_probe simple|zero_equal|asymmetric|nonzero_source|large_cost|random_mixed|parallel_identity\n");
    return 2;
  }
  int ok = 0;
  if (strcmp(argv[1], "simple") == 0) ok = case_simple();
  else if (strcmp(argv[1], "zero_equal") == 0) ok = case_zero_equal();
  else if (strcmp(argv[1], "asymmetric") == 0) ok = case_asymmetric();
  else if (strcmp(argv[1], "nonzero_source") == 0) ok = case_nonzero_source();
  else if (strcmp(argv[1], "large_cost") == 0) ok = case_large_cost();
  else if (strcmp(argv[1], "random_mixed") == 0) ok = case_random_mixed();
  else if (strcmp(argv[1], "parallel_identity") == 0) ok = case_parallel_identity();
  else {
    fprintf(stderr, "unknown case: %s\n", argv[1]);
    return 2;
  }
  printf("%s case=%s\n", ok ? "PASS" : "FAIL", argv[1]);
  return ok ? 0 : 3;
}
