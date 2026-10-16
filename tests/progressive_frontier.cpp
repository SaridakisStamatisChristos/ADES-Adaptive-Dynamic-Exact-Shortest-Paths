#include "ades/progressive_ades.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace ades;

static void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

static Graph fixture() {
  Graph g(9);
  g.add_edge(0, 1, 2);
  g.add_edge(0, 2, 5);
  g.add_edge(1, 2, 1);
  g.add_edge(1, 3, 4);
  g.add_edge(2, 3, 1);
  g.add_edge(3, 4, 2);
  g.add_edge(2, 5, 7);
  g.add_edge(4, 5, 1);
  g.add_edge(5, 6, 3);
  g.add_edge(1, 6, 20);
  g.add_edge(6, 7, 1);
  return g;  // vertex 8 is unreachable from source 0.
}

static void progressive_kernel_is_exact_and_completable() {
  auto g = fixture();
  const auto reference = dijkstra(g, 0);
  ProgressiveFrontier frontier(g.vertex_count(), 0);

  for (const auto target : {6u, 3u, 8u, 5u, 1u, 7u, 4u, 2u, 0u}) {
    const auto result = progressive_bidirectional_query(g, frontier, target);
    require(result.distance == reference.dist[target],
            "progressive query disagrees with Dijkstra");
  }
  const auto settled_before = frontier.settled_vertices;
  const auto state = complete_progressive_sssp(g, frontier);
  require(state.dist == reference.dist,
          "completed progressive SSSP disagrees with Dijkstra");
  require(frontier.complete, "completed frontier not marked complete");
  require(frontier.settled_vertices >= settled_before,
          "completion moved settled count backwards");
}

static void update_invalidates_partial_lazily_and_remains_exact() {
  auto g = fixture();
  ProgressiveConfig cfg;
  cfg.resident_cap = 4;
  cfg.partial_cap = 4;
  cfg.persistent_state_budget_bytes = 1u << 20;
  ProgressiveADES subject(g, cfg);

  require(subject.query(0, 7) == dijkstra(g, 0).dist[7],
          "V4 first query mismatch");
  require(subject.query(0, 6) == dijkstra(g, 0).dist[6],
          "V4 second query mismatch");
  require(subject.partial(0) || subject.resident(0),
          "V4 did not retain repeated-source work");

  const auto old = g.edge(0).weight;
  g.update_weight(0, old + 9);
  subject.update(0, old + 9);
  const auto expected = dijkstra(g, 0).dist[7];
  require(subject.query(0, 7) == expected,
          "V4 post-update exactness mismatch");
  require(subject.stats().partial_invalidations >= 1 || subject.resident(0),
          "V4 neither invalidated speculative state nor retained repairable state");
  require(subject.stats().timing.reconciles(),
          "V4 telemetry does not reconcile");
}

static void tiny_budget_falls_back_without_violation() {
  auto g = fixture();
  ProgressiveConfig cfg;
  cfg.resident_cap = 4;
  cfg.partial_cap = 4;
  // Enough for the fixed 64-slot recent table, intentionally too small for a
  // progressive source state.
  cfg.persistent_state_budget_bytes = 2048;
  ProgressiveADES subject(g, cfg);
  const auto expected = dijkstra(g, 0).dist[7];
  for (int i = 0; i < 8; ++i) require(subject.query(0, 7) == expected,
                                      "tiny-budget V4 exactness mismatch");
  require(subject.partial_count() == 0,
          "tiny budget unexpectedly retained progressive state");
  require(subject.peak_accounted_algorithm_state_bytes() <=
              cfg.persistent_state_budget_bytes,
          "tiny-budget V4 exceeded logical byte budget");
}

static void randomized_dynamic_differential() {
  for (std::uint64_t seed = 0; seed < 6; ++seed) {
    std::mt19937_64 rng(seed + 901);
    Graph reference(32);
    for (int i = 0; i < 220; ++i) {
      const auto u = static_cast<std::uint32_t>(rng() % 32);
      const auto v = static_cast<std::uint32_t>(rng() % 32);
      if (u != v) reference.add_edge(u, v, 1 + rng() % 40);
    }
    ProgressiveConfig cfg;
    cfg.resident_cap = 4;
    cfg.partial_cap = 6;
    cfg.persistent_state_budget_bytes = 1u << 20;
    ProgressiveADES subject(reference, cfg);

    for (int op = 0; op < 2500; ++op) {
      if (reference.edge_count() && (rng() % 4 == 0)) {
        const auto id = static_cast<std::uint32_t>(rng() % reference.edge_count());
        const auto weight = static_cast<Weight>(1 + rng() % 40);
        reference.update_weight(id, weight);
        subject.update(id, weight);
      } else {
        const auto s = static_cast<std::uint32_t>(rng() % 32);
        const auto t = static_cast<std::uint32_t>(rng() % 32);
        const auto expected = dijkstra(reference, s).dist[t];
        const auto got = subject.query(s, t);
        if (got != expected) {
          std::cerr << "V4 differential mismatch seed=" << seed
                    << " op=" << op << " source=" << s << " target=" << t
                    << " expected=" << expected << " got=" << got << '\n';
          throw std::runtime_error("V4 randomized differential failure");
        }
      }
      require(subject.peak_accounted_algorithm_state_bytes() <=
                  cfg.persistent_state_budget_bytes,
              "V4 randomized run exceeded byte budget");
    }
  }
}

int main() {
  progressive_kernel_is_exact_and_completable();
  update_invalidates_partial_lazily_and_remains_exact();
  tiny_budget_falls_back_without_violation();
  randomized_dynamic_differential();
  return 0;
}
