#include "ades/ades.hpp"
#include "ades/trace.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace ades;
using Clock = std::chrono::steady_clock;

struct Step {
  std::uint32_t edge;
  std::uint32_t target;
  Weight old_weight;
  Weight new_weight;
};

static std::vector<std::uint32_t> subtree_sizes(const Graph& g,
                                                const SSSPState& s) {
  std::vector<std::uint32_t> sizes(g.vertex_count(), 1), order;
  order.reserve(g.vertex_count());
  std::vector<std::uint32_t> stack{s.source};
  while (!stack.empty()) {
    const auto u = stack.back();
    stack.pop_back();
    order.push_back(u);
    for (auto child = s.first_child[u]; child >= 0;
         child = s.next_sibling[static_cast<std::uint32_t>(child)]) {
      stack.push_back(static_cast<std::uint32_t>(child));
    }
  }
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    const auto v = *it;
    if (v != s.source && s.parent_edge[v] >= 0) {
      sizes[g.edge(static_cast<std::uint32_t>(s.parent_edge[v])).from] += sizes[v];
    }
  }
  return sizes;
}

static std::uint32_t pick_edge(const Graph& g, const SSSPState& s,
                               const std::vector<std::uint32_t>& sizes,
                               bool catastrophic, std::mt19937_64& rng) {
  std::vector<std::uint32_t> ids;
  for (std::uint32_t v = 0; v < g.vertex_count(); ++v) {
    if (s.parent_edge[v] < 0) continue;
    const auto subtree = sizes[v];
    const bool eligible = catastrophic
                              ? subtree >= std::max<std::size_t>(2, g.vertex_count() / 4)
                              : subtree <= std::max<std::size_t>(8, g.vertex_count() / 1000);
    if (eligible) ids.push_back(static_cast<std::uint32_t>(s.parent_edge[v]));
  }
  if (ids.empty()) {
    for (std::uint32_t v = 0; v < g.vertex_count(); ++v) {
      if (s.parent_edge[v] >= 0) {
        ids.push_back(static_cast<std::uint32_t>(s.parent_edge[v]));
      }
    }
  }
  if (ids.empty()) {
    std::cerr << "no reachable SPT edge\n";
    std::exit(4);
  }
  return ids[rng() % ids.size()];
}

int main(int argc, char** argv) {
  try {
    if (argc < 3) {
      std::cerr << "usage: ades_controller_workload graph small|catastrophic "
                   "[rounds] [seed] [policy]\n";
      return 2;
    }

    auto base = Graph::load_dimacs_gr_gz(argv[1]);
    const std::string regime = argv[2];
    const std::string policy = argc > 5 ? argv[5] : "work";
    const std::size_t rounds = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 20;
    const std::uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 7;
    if (regime != "small" && regime != "catastrophic") {
      std::cerr << "unknown regime\n";
      return 2;
    }

    std::mt19937_64 rng(seed);
    std::uint32_t source = static_cast<std::uint32_t>(seed % base.vertex_count());
    auto model = dijkstra(base, source);
    for (std::size_t tries = 0; tries < base.vertex_count(); ++tries) {
      bool any = false;
      for (auto parent : model.parent_edge) {
        if (parent >= 0) {
          any = true;
          break;
        }
      }
      if (any) break;
      source = (source + 1) % base.vertex_count();
      model = dijkstra(base, source);
    }

    std::vector<Step> steps;
    steps.reserve(rounds);
    for (std::size_t i = 0; i < rounds; ++i) {
      const auto sizes = subtree_sizes(base, model);
      const auto id = pick_edge(base, model, sizes, regime == "catastrophic", rng);
      const auto edge = base.edge(id);
      const Weight new_weight = edge.weight + 1 + (rng() % 17);
      base.update_weight(id, new_weight);
      model = dijkstra(base, source);

      std::vector<std::uint32_t> reachable;
      reachable.reserve(base.vertex_count());
      for (std::uint32_t v = 0; v < base.vertex_count(); ++v) {
        if (model.dist[v] < INF) reachable.push_back(v);
      }
      if (reachable.empty()) {
        std::cerr << "resident source has no reachable target\n";
        return 4;
      }
      const auto target = reachable[rng() % reachable.size()];
      steps.push_back({id, target, edge.weight, new_weight});
    }

    const auto initial = Graph::load_dimacs_gr_gz(argv[1]);
    std::vector<TraceOp> trace;
    trace.reserve(steps.size() * 2);
    for (const auto& step : steps) {
      trace.push_back(TraceOp::update(step.edge, step.old_weight, step.new_weight));
      trace.push_back(TraceOp::query(source, step.target));
    }
    validate_trace_against_graph(initial, trace);
    const auto sha = trace_sha256(trace);
    const auto counts = trace_counts(trace);

    auto graph = initial;
    Config cfg;
    cfg.resident_cap = 1;
    cfg.probation_queries = 1;
    cfg.promotion_ratio = 0.0;
    cfg.repair_safety_ceiling = {graph.vertex_count(), graph.edge_count()};
    if (policy == "fixed")
      cfg.repair_policy = RepairPolicy::Fixed;
    else if (policy == "vertex")
      cfg.repair_policy = RepairPolicy::VertexOnly;
    else if (policy == "work")
      cfg.repair_policy = RepairPolicy::WorkAware;
    else
      return 2;

    ADES ades(graph, cfg);
    if (ades.query(source, source) != 0 || !ades.resident(source)) {
      std::cerr << "failed to establish resident source\n";
      return 5;
    }

    std::vector<Distance> expected;
    expected.reserve(steps.size());
    for (const auto& step : steps) {
      graph.update_weight(step.edge, step.new_weight);
      expected.push_back(dijkstra(graph, source).dist[step.target]);
    }

    std::uint64_t ns = 0;
    for (std::size_t i = 0; i < steps.size(); ++i) {
      const auto& step = steps[i];
      const auto start = Clock::now();
      ades.update(step.edge, step.new_weight);
      const auto got = ades.query(source, step.target);
      ns += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
              .count());
      if (got != expected[i]) {
        std::cerr << "exactness failure\n";
        return 3;
      }
    }

    const auto stats = ades.stats();
    std::cout << regime << ',' << policy << ',' << seed << ',' << sha << ','
              << counts.query_count << ',' << counts.update_count << ','
              << counts.increase_count << ',' << counts.decrease_count << ','
              << source << ',' << ns << ',' << stats.rebuilds << ','
              << stats.increase_repairs << ',' << stats.repair_aborts << ','
              << stats.filtered_updates << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ades_controller_workload: " << error.what() << '\n';
    return 2;
  }
}
