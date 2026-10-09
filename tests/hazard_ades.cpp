#include "ades/hazard_ades.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace ades;

static void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

static Graph fixture() {
  Graph g(12);
  for (std::uint32_t i = 0; i + 1 < 12; ++i) g.add_edge(i, i + 1, 1);
  g.add_edge(0, 6, 4);
  g.add_edge(2, 9, 5);
  g.add_edge(1, 10, 12);
  return g;
}

static void update_between_touches_blocks_speculation() {
  auto reference = fixture();
  HazardConfig cfg;
  cfg.resident_cap = 0;  // isolate progressive-survival gate
  cfg.partial_cap = 4;
  cfg.persistent_state_budget_bytes = 1u << 20;
  HazardAwareADES subject(reference, cfg);

  require(subject.query(0, 11) == dijkstra(reference, 0).dist[11],
          "V5 first query mismatch");
  const auto old = reference.edge(0).weight;
  reference.update_weight(0, old + 2);
  subject.update(0, old + 2);
  require(subject.query(0, 10) == dijkstra(reference, 0).dist[10],
          "V5 post-update repeat mismatch");
  require(subject.partial_count() == 0,
          "V5 created speculative state across an update boundary");
  require(subject.stats().hazard_rejections >= 1,
          "V5 did not report the hazard rejection");
}

static void same_epoch_repeat_can_progress() {
  auto reference = fixture();
  HazardConfig cfg;
  cfg.resident_cap = 0;  // force the middle lane rather than direct full state
  cfg.partial_cap = 4;
  cfg.persistent_state_budget_bytes = 1u << 20;
  cfg.startup_max_reuse_gap = 4;
  HazardAwareADES subject(reference, cfg);
  require(subject.query(0, 11) == dijkstra(reference, 0).dist[11],
          "V5 cold query mismatch");
  require(subject.query(0, 10) == dijkstra(reference, 0).dist[10],
          "V5 progressive query mismatch");
  require(subject.partial(0), "V5 did not retain same-epoch progressive state");
  require(subject.stats().partial_creations == 1,
          "V5 progressive creation count mismatch");
}

static void tiny_budget_remains_exact() {
  auto reference = fixture();
  HazardConfig cfg;
  cfg.persistent_state_budget_bytes = 4096;
  HazardAwareADES subject(reference, cfg);
  for (int i = 0; i < 16; ++i)
    require(subject.query(0, 11) == dijkstra(reference, 0).dist[11],
            "V5 tiny-budget exactness mismatch");
  require(subject.peak_accounted_algorithm_state_bytes() <=
              cfg.persistent_state_budget_bytes,
          "V5 tiny-budget accounting violation");
}

static void randomized_dynamic_differential() {
  for (std::uint64_t seed = 0; seed < 6; ++seed) {
    std::mt19937_64 rng(seed + 1201);
    Graph reference(40);
    for (int i = 0; i < 320; ++i) {
      const auto u = static_cast<std::uint32_t>(rng() % 40);
      const auto v = static_cast<std::uint32_t>(rng() % 40);
      if (u != v) reference.add_edge(u, v, 1 + rng() % 50);
    }
    HazardConfig cfg;
    cfg.resident_cap = 4;
    cfg.partial_cap = 6;
    cfg.persistent_state_budget_bytes = 1u << 20;
    HazardAwareADES subject(reference, cfg);

    for (int op = 0; op < 3000; ++op) {
      if (reference.edge_count() && (rng() % 4 == 0)) {
        const auto id = static_cast<std::uint32_t>(rng() % reference.edge_count());
        const auto weight = static_cast<Weight>(1 + rng() % 50);
        reference.update_weight(id, weight);
        subject.update(id, weight);
      } else {
        const auto s = static_cast<std::uint32_t>(rng() % 40);
        const auto t = static_cast<std::uint32_t>(rng() % 40);
        const auto expected = dijkstra(reference, s).dist[t];
        const auto got = subject.query(s, t);
        if (got != expected) {
          std::cerr << "V5 mismatch seed=" << seed << " op=" << op
                    << " source=" << s << " target=" << t
                    << " expected=" << expected << " got=" << got << '\n';
          throw std::runtime_error("V5 randomized differential failure");
        }
      }
      require(subject.peak_accounted_algorithm_state_bytes() <=
                  cfg.persistent_state_budget_bytes,
              "V5 randomized byte-budget violation");
    }
  }
  require(true, "unreachable");
}

int main() {
  update_between_touches_blocks_speculation();
  same_epoch_repeat_can_progress();
  tiny_budget_remains_exact();
  randomized_dynamic_differential();
  return 0;
}
