#include "ades/dynamic_repair.hpp"
#include "ades/repair_controller.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace ades;

static void preflight_rejection_is_read_only() {
  Graph g(5);
  const auto changed = g.add_edge(0, 1, 1);
  g.add_edge(1, 2, 1);
  g.add_edge(2, 3, 1);
  g.add_edge(3, 4, 1);
  g.add_edge(0, 2, 50);
  g.add_edge(0, 3, 60);
  g.add_edge(0, 4, 70);
  g.freeze_topology();

  auto state = dijkstra(g, 0);
  const auto dist = state.dist;
  const auto parent = state.parent_edge;
  const auto first = state.first_child;
  const auto next = state.next_sibling;
  const auto prev = state.prev_sibling;
  const auto old = g.edge(changed);
  g.update_weight(changed, 10);

  RepairWork work{};
  std::size_t discovered = 0;
  const auto result = repair_increase(
      g, state, changed, old,
      RepairBudget{100, 100, 1}, &discovered, &work);

  assert(result == RepairResult::RebuildRequired);
  assert(discovered == 4);
  assert(state.dist == dist);
  assert(state.parent_edge == parent);
  assert(state.first_child == first);
  assert(state.next_sibling == next);
  assert(state.prev_sibling == prev);
}

static void accepted_repair_finishes_even_if_runtime_work_exceeds_prediction() {
  Graph g(5);
  const auto changed = g.add_edge(0, 1, 1);
  g.add_edge(1, 2, 1);
  g.add_edge(2, 3, 1);
  g.add_edge(3, 4, 1);
  g.add_edge(0, 2, 50);
  g.add_edge(0, 3, 60);
  g.add_edge(0, 4, 70);
  g.freeze_topology();

  auto state = dijkstra(g, 0);
  const auto old = g.edge(changed);
  g.update_weight(changed, 10);

  // Preflight prediction is:
  // V=4, tree=3, incoming scans=7, outgoing upper bound=3, useful pops=4.
  // Total = 21. The actual radix queue performs three additional stale pops,
  // so measured work is 24. Once preflight accepts, PR56 must finish instead
  // of aborting after semantic mutation and paying for a rebuild too.
  RepairWork work{};
  const auto result = repair_increase(
      g, state, changed, old,
      RepairBudget{100, 100, 21}, nullptr, &work);

  assert(result == RepairResult::Repaired);
  assert(work.total() > 21);
  const auto oracle = dijkstra(g, 0);
  assert(state.dist == oracle.dist);
}

static void work_controller_uses_hard_discovery_caps_and_economic_work_gate() {
  RepairController controller;
  auto cold = controller.budget();
  assert(cold.vertices == (1u << 20));
  assert(cold.tree_edges == (1u << 22));
  assert(cold.work_units == std::numeric_limits<std::uint64_t>::max());

  controller.observe_rebuild(1'000'000);
  RepairWork sample{};
  sample.discovered_vertices = 100;
  sample.tree_edges = 99;
  sample.boundary_scans = 200;
  sample.restricted_scans = 200;
  sample.pq_pops = 101;
  assert(sample.total() == 700);
  controller.observe_repair(7'000, sample);  // 10 ns/work unit.

  const auto learned = controller.budget();
  assert(learned.vertices == (1u << 20));
  assert(learned.tree_edges == (1u << 22));
  assert(learned.work_units == 90'000);  // 0.9 * 1,000,000 / 10.
}

int main() {
  preflight_rejection_is_read_only();
  accepted_repair_finishes_even_if_runtime_work_exceeds_prediction();
  work_controller_uses_hard_discovery_caps_and_economic_work_gate();
  return 0;
}
