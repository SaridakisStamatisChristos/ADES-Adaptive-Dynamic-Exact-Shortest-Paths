#include "ades/ades.hpp"
#include "ades/bounded_baselines.hpp"
#include "ades/progressive_ades.hpp"
#include "ades/trace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ades;
using Clock = std::chrono::steady_clock;

struct Timing {
  std::uint64_t algorithm_ns = 0;
  std::uint64_t query_ns = 0;
  std::uint64_t update_ns = 0;
  std::vector<std::uint64_t> all;
  std::vector<std::uint64_t> queries;
  std::vector<std::uint64_t> updates;
};

static std::uint64_t elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
      .count();
}

static std::uint64_t percentile(std::vector<std::uint64_t> values, double p) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const auto rank = static_cast<std::size_t>(
      std::ceil(p * static_cast<double>(values.size())));
  return values[std::min(values.size() - 1,
                         rank ? rank - 1 : std::size_t{0})];
}

static std::vector<Distance> build_oracle(Graph graph,
                                          const std::vector<TraceOp>& ops) {
  std::vector<Distance> answers;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update)
      graph.update_weight(op.a, op.new_weight);
    else
      answers.push_back(dijkstra(graph, op.a).dist.at(op.b));
  }
  return answers;
}

static void write_oracle(const std::string& path, const std::string& sha,
                         const std::vector<Distance>& answers) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write PR52 oracle bundle");
  out << "ADES_ORACLE_V1 " << sha << ' ' << answers.size() << '\n';
  for (const auto distance : answers) out << distance << '\n';
}

static std::vector<Distance> read_oracle(const std::string& path,
                                         const std::string& expected_sha,
                                         std::size_t expected_queries) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read PR52 oracle bundle");
  std::string magic, sha;
  std::size_t count = 0;
  if (!(in >> magic >> sha >> count) || magic != "ADES_ORACLE_V1")
    throw std::runtime_error("malformed PR52 oracle bundle");
  if (sha != expected_sha || count != expected_queries)
    throw std::runtime_error("PR52 oracle identity mismatch");
  std::vector<Distance> answers(count);
  for (auto& answer : answers)
    if (!(in >> answer)) throw std::runtime_error("truncated PR52 oracle bundle");
  return answers;
}

template <class Engine>
static Timing run_engine(Engine& engine, const std::vector<TraceOp>& ops,
                         const std::vector<Distance>& oracle) {
  Timing timing;
  timing.all.reserve(ops.size());
  timing.queries.reserve(oracle.size());
  timing.updates.reserve(ops.size() - oracle.size());
  std::size_t qi = 0;
  for (const auto& op : ops) {
    const auto start = Clock::now();
    if (op.kind == TraceOpKind::Update) {
      engine.update(op.a, op.new_weight);
      const auto ns = elapsed_ns(start);
      timing.update_ns += ns;
      timing.updates.push_back(ns);
      timing.all.push_back(ns);
    } else {
      const auto answer = engine.query(op.a, op.b);
      const auto ns = elapsed_ns(start);
      timing.query_ns += ns;
      timing.queries.push_back(ns);
      timing.all.push_back(ns);
      if (qi >= oracle.size() || answer != oracle[qi++])
        throw std::runtime_error("PR52 exactness failure");
    }
  }
  if (qi != oracle.size()) throw std::runtime_error("PR52 oracle consumption mismatch");
  timing.algorithm_ns = timing.query_ns + timing.update_ns;
  return timing;
}

struct RowStats {
  std::uint64_t current_bytes = 0;
  std::uint64_t peak_bytes = 0;
  std::uint64_t cold_queries = 0;
  std::uint64_t resident_queries = 0;
  std::uint64_t promotions = 0;
  std::uint64_t evictions = 0;
  std::uint64_t rebuilds = 0;
  std::uint64_t repair_aborts = 0;
  std::uint64_t filtered_updates = 0;
  std::uint64_t decrease_repairs = 0;
  std::uint64_t increase_repairs = 0;
  std::uint64_t memory_budget_rejections = 0;
  std::uint64_t progressive_queries = 0;
  std::uint64_t partial_creations = 0;
  std::uint64_t partial_evictions = 0;
  std::uint64_t partial_invalidations = 0;
  std::uint64_t partial_completions = 0;
  std::uint64_t progressive_promotions = 0;
};

static ScientificAblation parse_ablation(const std::string& profile) {
  if (profile == "COLD") return ScientificAblation::Cold;
  if (profile == "FREQ-LRU-REPAIR") return ScientificAblation::FreqLruRepair;
  if (profile == "ADES") return ScientificAblation::FullADES;
  if (profile == "ADES-V2") return ScientificAblation::PredictiveEconomic;
  if (profile == "ADES-V3") return ScientificAblation::PredictiveEconomicFast;
  throw std::invalid_argument("unknown PR52 historical profile: " + profile);
}

static void emit(const std::string& profile, const std::string& trace_sha,
                 const TraceCounts& counts, std::uint64_t budget,
                 const Timing& timing, const RowStats& stats) {
  std::cout << profile << ',' << trace_sha << ',' << counts.query_count << ','
            << counts.update_count << ',' << counts.increase_count << ','
            << counts.decrease_count << ',' << budget << ','
            << timing.algorithm_ns << ',' << timing.query_ns << ','
            << timing.update_ns << ',' << percentile(timing.all, 0.50) << ','
            << percentile(timing.all, 0.95) << ','
            << percentile(timing.queries, 0.50) << ','
            << percentile(timing.queries, 0.95) << ','
            << percentile(timing.updates, 0.50) << ','
            << percentile(timing.updates, 0.95) << ',' << stats.current_bytes
            << ',' << stats.peak_bytes << ',' << stats.cold_queries << ','
            << stats.resident_queries << ',' << stats.promotions << ','
            << stats.evictions << ',' << stats.rebuilds << ','
            << stats.repair_aborts << ',' << stats.filtered_updates << ','
            << stats.decrease_repairs << ',' << stats.increase_repairs << ','
            << stats.memory_budget_rejections << ','
            << stats.progressive_queries << ',' << stats.partial_creations << ','
            << stats.partial_evictions << ',' << stats.partial_invalidations << ','
            << stats.partial_completions << ','
            << stats.progressive_promotions << '\n';
}

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage:\n"
                   "  ades_pr52_profile oracle GRAPH TRACE ORACLE_OUT\n"
                   "  ades_pr52_profile run GRAPH TRACE ORACLE_FILE BUDGET PROFILE\n";
      return 2;
    }
    const std::string mode = argv[1];
    if (mode == "oracle") {
      if (argc != 5) throw std::invalid_argument("oracle mode requires GRAPH TRACE ORACLE_OUT");
      auto graph = Graph::load_dimacs_gr_gz(argv[2]);
      auto ops = read_trace(argv[3]);
      validate_trace_against_graph(graph, ops);
      const auto sha = trace_sha256(ops);
      auto answers = build_oracle(graph, ops);
      write_oracle(argv[4], sha, answers);
      std::cout << "ORACLE trace_sha256=" << sha << " queries=" << answers.size()
                << '\n';
      return 0;
    }
    if (mode != "run" || argc != 7)
      throw std::invalid_argument("run mode requires GRAPH TRACE ORACLE_FILE BUDGET PROFILE");

    auto graph = Graph::load_dimacs_gr_gz(argv[2]);
    auto ops = read_trace(argv[3]);
    validate_trace_against_graph(graph, ops);
    const auto sha = trace_sha256(ops);
    const auto counts = trace_counts(ops);
    auto oracle = read_oracle(argv[4], sha, counts.query_count);
    const std::uint64_t budget = std::stoull(argv[5]);
    if (!budget) throw std::invalid_argument("PR52 budget must be positive");
    const std::string profile = argv[6];

    RowStats row;
    Timing timing;
    if (profile == "B3L") {
      BoundedResident engine(graph, ResidentMode::LocalRepair,
                             std::max<std::size_t>(1, graph.vertex_count()),
                             budget);
      timing = run_engine(engine, ops, oracle);
      row.current_bytes = engine.accounted_algorithm_state_bytes();
      row.peak_bytes = engine.peak_accounted_algorithm_state_bytes();
      row.cold_queries = engine.misses();
      row.resident_queries = engine.hits();
      row.evictions = engine.evictions();
    } else if (profile == "ADES-V4") {
      ProgressiveConfig config;
      config.resident_cap = std::max<std::size_t>(1, graph.vertex_count());
      config.partial_cap = 8;
      config.persistent_state_budget_bytes = budget;
      ProgressiveADES engine(graph, config);
      timing = run_engine(engine, ops, oracle);
      const auto& stats = engine.stats();
      if (!stats.timing.reconciles())
        throw std::logic_error("ADES-V4 telemetry does not reconcile");
      row.current_bytes = stats.accounted_algorithm_state_bytes;
      row.peak_bytes = stats.peak_accounted_algorithm_state_bytes;
      row.cold_queries = stats.cold_queries;
      row.resident_queries = stats.resident_queries;
      row.promotions = stats.promotions;
      row.evictions = stats.evictions;
      row.rebuilds = stats.rebuilds;
      row.repair_aborts = stats.repair_aborts;
      row.filtered_updates = stats.filtered_updates;
      row.decrease_repairs = stats.decrease_repairs;
      row.increase_repairs = stats.increase_repairs;
      row.memory_budget_rejections = stats.memory_budget_rejections;
      row.progressive_queries = stats.progressive_queries;
      row.partial_creations = stats.partial_creations;
      row.partial_evictions = stats.partial_evictions;
      row.partial_invalidations = stats.partial_invalidations;
      row.partial_completions = stats.partial_completions;
      row.progressive_promotions = stats.progressive_promotions;
    } else {
      Config base;
      base.resident_cap = std::max<std::size_t>(1, graph.vertex_count());
      base.persistent_state_budget_bytes = budget;
      auto config = config_for_scientific_ablation(parse_ablation(profile), base);
      ADES engine(graph, config);
      timing = run_engine(engine, ops, oracle);
      const auto& stats = engine.stats();
      if (!stats.timing.reconciles())
        throw std::logic_error("historical ADES telemetry does not reconcile");
      row.current_bytes = stats.accounted_algorithm_state_bytes;
      row.peak_bytes = stats.peak_accounted_algorithm_state_bytes;
      row.cold_queries = stats.cold_queries;
      row.resident_queries = stats.resident_queries;
      row.promotions = stats.promotions;
      row.evictions = stats.evictions;
      row.rebuilds = stats.rebuilds;
      row.repair_aborts = stats.repair_aborts;
      row.filtered_updates = stats.filtered_updates;
      row.decrease_repairs = stats.decrease_repairs;
      row.increase_repairs = stats.increase_repairs;
      row.memory_budget_rejections = stats.memory_budget_rejections;
    }

    if (timing.algorithm_ns != timing.query_ns + timing.update_ns)
      throw std::logic_error("PR52 top-level timing does not reconcile");
    if (row.peak_bytes > budget)
      throw std::logic_error("PR52 byte-budget violation");
    emit(profile, sha, counts, budget, timing, row);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ades_pr52_profile: " << error.what() << '\n';
    return 2;
  }
}
