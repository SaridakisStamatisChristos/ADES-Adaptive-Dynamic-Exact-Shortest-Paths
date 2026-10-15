#include "ades/dynamic_repair.hpp"
#include "ades/repair_controller.hpp"
#include "ades/shortest_paths.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using ades::Distance;
using ades::Graph;
using ades::RepairController;
using ades::RepairResult;
using ades::RepairWork;
using ades::SSSPState;
using ades::Weight;

struct Update {
  std::uint32_t edge_id{};
  Weight old_weight{};
  Weight new_weight{};
};

std::uint64_t ns_since(Clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}

struct Input {
  Graph graph;
  std::uint32_t source{};
  std::vector<Update> updates;
};

Input load_input(const std::string& graph_path, const std::string& updates_path) {
  std::ifstream gin(graph_path);
  if (!gin) throw std::runtime_error("cannot open graph file");
  std::size_t n = 0, m = 0;
  std::uint32_t source = 0;
  if (!(gin >> n >> m >> source)) throw std::runtime_error("malformed graph header");
  Graph graph(n);
  for (std::size_t i = 0; i < m; ++i) {
    std::uint32_t id = 0, u = 0, v = 0;
    Weight w = 0;
    if (!(gin >> id >> u >> v >> w)) throw std::runtime_error("malformed graph edge");
    if (id != i) throw std::runtime_error("graph edge ids must be contiguous and ordered");
    const auto actual = graph.add_edge(u, v, w);
    if (actual != id) throw std::runtime_error("ADES edge id mismatch");
  }
  graph.freeze_topology();

  std::ifstream uin(updates_path);
  if (!uin) throw std::runtime_error("cannot open updates file");
  std::size_t k = 0;
  if (!(uin >> k)) throw std::runtime_error("malformed update header");
  std::vector<Update> updates;
  updates.reserve(k);
  for (std::size_t i = 0; i < k; ++i) {
    Update u;
    if (!(uin >> u.edge_id >> u.old_weight >> u.new_weight))
      throw std::runtime_error("malformed update row");
    if (u.edge_id >= graph.edge_count()) throw std::runtime_error("update edge out of range");
    if (u.old_weight == 0 || u.new_weight == 0) throw std::runtime_error("weights must be positive");
    updates.push_back(u);
  }
  if (source >= n) throw std::runtime_error("source out of range");
  return {std::move(graph), source, std::move(updates)};
}

std::uint64_t max_finite_distance(const SSSPState& state) {
  std::uint64_t mx = 0;
  for (auto d : state.dist) {
    if (d < ades::INF) mx = std::max<std::uint64_t>(mx, d);
  }
  return mx;
}

bool same_distances(const SSSPState& a, const SSSPState& b, std::uint32_t* vertex = nullptr) {
  if (a.dist.size() != b.dist.size()) return false;
  for (std::size_t i = 0; i < a.dist.size(); ++i) {
    if (a.dist[i] != b.dist[i]) {
      if (vertex) *vertex = static_cast<std::uint32_t>(i);
      return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 4) {
      std::cerr << "usage: pr70_ades_fixed_source <graph.tsv> <updates.tsv> <exact|timed>\n";
      return 2;
    }
    const bool exact_mode = std::string(argv[3]) == "exact";
    if (!exact_mode && std::string(argv[3]) != "timed") {
      throw std::runtime_error("mode must be exact or timed");
    }

    auto input = load_input(argv[1], argv[2]);
    auto init_start = Clock::now();
    SSSPState state = ades::dijkstra(input.graph, input.source);
    const auto initialization_ns = ns_since(init_start);

    RepairController controller;
    controller.observe_rebuild(initialization_ns);

    std::uint64_t maintenance_ns = 0;
    std::uint64_t repaired = 0, filtered = 0, rebuilds = 0;
    std::uint64_t decreases = 0, increases = 0;
    std::uint64_t max_distance = max_finite_distance(state);
    bool exact = true;
    long long first_mismatch_update = -1;
    std::uint32_t first_mismatch_vertex = std::numeric_limits<std::uint32_t>::max();

    if (max_distance >= 65535) {
      std::cerr << "initial state exceeds GraphBolt uint16 distance boundary\n";
      return 3;
    }

    constexpr std::size_t kSafetyVertices = 1u << 20;
    constexpr std::size_t kSafetyTreeEdges = 1u << 22;

    for (std::size_t i = 0; i < input.updates.size(); ++i) {
      const auto& u = input.updates[i];
      const auto old_edge = input.graph.edge(u.edge_id);
      if (old_edge.weight != u.old_weight) {
        throw std::runtime_error("update old-weight mismatch");
      }

      auto update_start = Clock::now();
      input.graph.update_weight(u.edge_id, u.new_weight);
      RepairResult result = RepairResult::RebuildRequired;
      RepairWork work{};
      std::size_t discovered = 0;

      if (u.new_weight < u.old_weight) {
        ++decreases;
        result = ades::repair_decrease(input.graph, state, u.edge_id);
      } else {
        ++increases;
        const auto budget = controller.budget();
        const ades::RepairBudget bounded{
            std::min<std::size_t>(budget.vertices, kSafetyVertices),
            std::min<std::size_t>(budget.tree_edges, kSafetyTreeEdges),
            budget.work_units};
        auto repair_start = Clock::now();
        result = ades::repair_increase(input.graph, state, u.edge_id, old_edge,
                                       bounded, &discovered, &work);
        const auto repair_ns = ns_since(repair_start);
        if (result == RepairResult::Repaired) controller.observe_repair(repair_ns, work);
      }

      if (result == RepairResult::Filtered) {
        ++filtered;
      } else if (result == RepairResult::Repaired) {
        ++repaired;
      } else {
        auto rebuild_start = Clock::now();
        state = ades::dijkstra(input.graph, input.source);
        const auto rebuild_ns = ns_since(rebuild_start);
        controller.observe_rebuild(rebuild_ns);
        ++rebuilds;
      }
      maintenance_ns += ns_since(update_start);

      max_distance = std::max(max_distance, max_finite_distance(state));
      if (max_distance >= 65535) {
        std::cerr << "state exceeds GraphBolt uint16 distance boundary at update " << i << "\n";
        return 3;
      }

      if (exact_mode) {
        const auto oracle = ades::dijkstra(input.graph, input.source);
        std::uint32_t mismatch_vertex = 0;
        if (!same_distances(state, oracle, &mismatch_vertex)) {
          exact = false;
          first_mismatch_update = static_cast<long long>(i);
          first_mismatch_vertex = mismatch_vertex;
          break;
        }
      }
    }

    const auto total_ns = initialization_ns + maintenance_ns;
    std::cout << "schema=ades-pr70-fixed-source-runner-v1\n";
    std::cout << "mode=" << (exact_mode ? "exact" : "timed") << "\n";
    std::cout << "source=" << input.source << "\n";
    std::cout << "updates=" << input.updates.size() << "\n";
    std::cout << "initialization_ns=" << initialization_ns << "\n";
    std::cout << "maintenance_ns=" << maintenance_ns << "\n";
    std::cout << "total_ns=" << total_ns << "\n";
    std::cout << "decreases=" << decreases << "\n";
    std::cout << "increases=" << increases << "\n";
    std::cout << "repaired=" << repaired << "\n";
    std::cout << "filtered=" << filtered << "\n";
    std::cout << "rebuilds=" << rebuilds << "\n";
    std::cout << "max_finite_distance=" << max_distance << "\n";
    std::cout << "exact=" << (exact ? 1 : 0) << "\n";
    std::cout << "first_mismatch_update=" << first_mismatch_update << "\n";
    std::cout << "first_mismatch_vertex=" << first_mismatch_vertex << "\n";
    return exact ? 0 : 4;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
