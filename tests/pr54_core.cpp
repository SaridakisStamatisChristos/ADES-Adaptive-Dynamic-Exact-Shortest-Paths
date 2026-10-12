#include "ades/dynamic_repair.hpp"
#include "ades/shortest_paths.hpp"

#include <cassert>
#include <cstdint>
#include <random>

using namespace ades;

static void tight_nonparent_increase_is_read_only() {
  Graph g(4);
  g.add_edge(0, 1, 1);
  g.add_edge(0, 2, 1);
  g.add_edge(1, 3, 1);
  const auto changed = g.add_edge(2, 3, 1);

  auto state = dijkstra(g, 0);
  assert(state.dist[3] == 2);
  assert(state.parent_edge[3] != static_cast<std::int64_t>(changed));

  const auto before_dist = state.dist;
  const auto before_parent = state.parent_edge;
  const auto before_first = state.first_child;
  const auto before_next = state.next_sibling;
  const auto before_prev = state.prev_sibling;
  const auto old = g.edge(changed);
  g.update_weight(changed, 100);

  RepairWork work{};
  std::size_t discovered = 99;
  const auto result = repair_increase(g, state, changed, old, {1, 1}, &discovered, &work);
  assert(result == RepairResult::Filtered);
  assert(discovered == 0);
  assert(work.total() == 0);
  assert(state.dist == before_dist);
  assert(state.parent_edge == before_parent);
  assert(state.first_child == before_first);
  assert(state.next_sibling == before_next);
  assert(state.prev_sibling == before_prev);
  assert(state.dist == dijkstra(g, 0).dist);
}

static void parallel_tight_nonparent_increase_is_filtered() {
  Graph g(3);
  const auto selected_candidate = g.add_edge(0, 1, 1);
  const auto parallel = g.add_edge(0, 1, 1);
  g.add_edge(1, 2, 1);
  auto state = dijkstra(g, 0);
  assert(state.parent_edge[1] == static_cast<std::int64_t>(selected_candidate));
  assert(state.parent_edge[1] != static_cast<std::int64_t>(parallel));
  const auto old = g.edge(parallel);
  g.update_weight(parallel, 50);
  assert(repair_increase(g, state, parallel, old, {100, 100}) == RepairResult::Filtered);
  assert(state.dist == dijkstra(g, 0).dist);
}

static void zero_weight_equal_witness_is_filtered() {
  Graph g(4);
  g.add_edge(0, 1, 0);
  g.add_edge(0, 2, 0);
  g.add_edge(1, 3, 1);
  const auto changed = g.add_edge(2, 3, 1);
  auto state = dijkstra(g, 0);
  assert(state.parent_edge[3] != static_cast<std::int64_t>(changed));
  const auto old = g.edge(changed);
  g.update_weight(changed, 2);
  assert(repair_increase(g, state, changed, old, {100, 100}) == RepairResult::Filtered);
  assert(state.dist == dijkstra(g, 0).dist);
}

static void randomized_tight_nonparent_filter_differential() {
  for (std::uint64_t seed = 0; seed < 40; ++seed) {
    std::mt19937_64 rng(seed);
    Graph base(18);
    for (int i = 0; i < 150; ++i) {
      const auto u = static_cast<std::uint32_t>(rng() % 18);
      const auto v = static_cast<std::uint32_t>(rng() % 18);
      if (u != v) base.add_edge(u, v, static_cast<Weight>(rng() % 8));
    }

    for (std::uint32_t source = 0; source < 18; ++source) {
      const auto original_state = dijkstra(base, source);
      for (std::uint32_t edge_id = 0; edge_id < base.edge_count(); ++edge_id) {
        const auto old = base.edge(edge_id);
        if (original_state.dist[old.from] >= INF) continue;
        if (sat_add(original_state.dist[old.from], old.weight) != original_state.dist[old.to]) continue;
        if (original_state.parent_edge[old.to] == static_cast<std::int64_t>(edge_id)) continue;

        Graph changed_graph = base;
        auto state = original_state;
        changed_graph.update_weight(edge_id, old.weight + 1 + static_cast<Weight>(rng() % 9));
        const auto result = repair_increase(changed_graph, state, edge_id, old, {1000000, 1000000});
        assert(result == RepairResult::Filtered);
        assert(state.dist == original_state.dist);
        assert(state.dist == dijkstra(changed_graph, source).dist);
      }
    }
  }
}

static void reusable_bidirectional_workspace_differential() {
  for (std::uint64_t seed = 100; seed < 112; ++seed) {
    std::mt19937_64 rng(seed);
    Graph g(64);
    for (int i = 0; i < 640; ++i) {
      const auto u = static_cast<std::uint32_t>(rng() % 64);
      const auto v = static_cast<std::uint32_t>(rng() % 64);
      if (u != v) g.add_edge(u, v, static_cast<Weight>(rng() % 32));
    }
    for (int op = 0; op < 3000; ++op) {
      if (g.edge_count() && op % 11 == 0) {
        const auto edge_id = static_cast<std::uint32_t>(rng() % g.edge_count());
        g.update_weight(edge_id, static_cast<Weight>(rng() % 32));
        continue;
      }
      const auto source = static_cast<std::uint32_t>(rng() % 64);
      const auto target = static_cast<std::uint32_t>(rng() % 64);
      const auto expected = dijkstra(g, source).dist[target];
      const auto result = bidirectional_dijkstra_profiled(g, source, target);
      assert(result.distance == expected);
    }
  }
}

int main() {
  tight_nonparent_increase_is_read_only();
  parallel_tight_nonparent_increase_is_filtered();
  zero_weight_equal_witness_is_filtered();
  randomized_tight_nonparent_filter_differential();
  reusable_bidirectional_workspace_differential();
}
