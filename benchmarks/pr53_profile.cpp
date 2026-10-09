#include "ades/ades.hpp"
#include "ades/bounded_baselines.hpp"
#include "ades/hazard_ades.hpp"
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
  std::uint64_t algorithm_ns = 0, query_ns = 0, update_ns = 0;
  std::vector<std::uint64_t> all, queries, updates;
};

static std::uint64_t elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

static std::uint64_t percentile(std::vector<std::uint64_t> values, double p) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const auto rank = static_cast<std::size_t>(std::ceil(p * values.size()));
  return values[std::min(values.size() - 1, rank ? rank - 1 : std::size_t{0})];
}

static std::vector<Distance> build_oracle(Graph graph, const std::vector<TraceOp>& ops) {
  std::vector<Distance> out;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) graph.update_weight(op.a, op.new_weight);
    else out.push_back(dijkstra(graph, op.a).dist.at(op.b));
  }
  return out;
}

static void write_oracle(const std::string& path, const std::string& sha,
                         const std::vector<Distance>& answers) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write PR53 oracle");
  out << "ADES_ORACLE_V1 " << sha << ' ' << answers.size() << '\n';
  for (auto d : answers) out << d << '\n';
}

static std::vector<Distance> read_oracle(const std::string& path,
                                         const std::string& expected_sha,
                                         std::size_t expected_queries) {
  std::ifstream in(path, std::ios::binary);
  std::string magic, sha; std::size_t count = 0;
  if (!(in >> magic >> sha >> count) || magic != "ADES_ORACLE_V1" ||
      sha != expected_sha || count != expected_queries)
    throw std::runtime_error("PR53 oracle identity mismatch");
  std::vector<Distance> answers(count);
  for (auto& d : answers) if (!(in >> d)) throw std::runtime_error("truncated PR53 oracle");
  return answers;
}

template <class Engine>
static Timing run_engine(Engine& engine, const std::vector<TraceOp>& ops,
                         const std::vector<Distance>& oracle) {
  Timing t; std::size_t qi = 0;
  for (const auto& op : ops) {
    const auto start = Clock::now();
    if (op.kind == TraceOpKind::Update) {
      engine.update(op.a, op.new_weight);
      const auto ns = elapsed_ns(start); t.update_ns += ns; t.updates.push_back(ns); t.all.push_back(ns);
    } else {
      const auto answer = engine.query(op.a, op.b);
      const auto ns = elapsed_ns(start); t.query_ns += ns; t.queries.push_back(ns); t.all.push_back(ns);
      if (qi >= oracle.size() || answer != oracle[qi++]) throw std::runtime_error("PR53 exactness failure");
    }
  }
  if (qi != oracle.size()) throw std::runtime_error("PR53 oracle consumption mismatch");
  t.algorithm_ns = t.query_ns + t.update_ns; return t;
}

struct Row {
  std::uint64_t current=0, peak=0, cold=0, resident=0, promotions=0, evictions=0,
      rebuilds=0, memory_rejections=0, progressive=0, partial_creations=0,
      partial_invalidations=0, progressive_promotions=0, direct_promotions=0,
      hazard_rejections=0, direct_fit_rejections=0;
};

static ScientificAblation ablation(const std::string& p) {
  if (p=="COLD") return ScientificAblation::Cold;
  if (p=="FREQ-LRU-REPAIR") return ScientificAblation::FreqLruRepair;
  if (p=="ADES-V2") return ScientificAblation::PredictiveEconomic;
  if (p=="ADES-V3") return ScientificAblation::PredictiveEconomicFast;
  if (p=="ADES") return ScientificAblation::FullADES;
  throw std::invalid_argument("unknown historical profile");
}

static void emit(const std::string& profile, const std::string& sha,
                 const TraceCounts& counts, std::uint64_t budget,
                 const Timing& t, const Row& r) {
  std::cout << profile << ',' << sha << ',' << counts.query_count << ',' << counts.update_count
            << ',' << budget << ',' << t.algorithm_ns << ',' << t.query_ns << ',' << t.update_ns
            << ',' << percentile(t.queries,.5) << ',' << percentile(t.queries,.95)
            << ',' << r.current << ',' << r.peak << ',' << r.cold << ',' << r.resident
            << ',' << r.promotions << ',' << r.evictions << ',' << r.rebuilds
            << ',' << r.memory_rejections << ',' << r.progressive << ',' << r.partial_creations
            << ',' << r.partial_invalidations << ',' << r.progressive_promotions
            << ',' << r.direct_promotions << ',' << r.hazard_rejections
            << ',' << r.direct_fit_rejections << '\n';
}

int main(int argc, char** argv) {
  try {
    if (argc < 2) return 2;
    const std::string mode=argv[1];
    if (mode=="oracle") {
      if (argc!=5) throw std::invalid_argument("oracle args");
      auto graph=Graph::load_dimacs_gr_gz(argv[2]); auto ops=read_trace(argv[3]);
      validate_trace_against_graph(graph,ops); const auto sha=trace_sha256(ops);
      write_oracle(argv[4],sha,build_oracle(graph,ops)); return 0;
    }
    if (mode!="run" || argc!=7) throw std::invalid_argument("run args");
    auto graph=Graph::load_dimacs_gr_gz(argv[2]); auto ops=read_trace(argv[3]);
    validate_trace_against_graph(graph,ops); const auto sha=trace_sha256(ops); const auto counts=trace_counts(ops);
    auto oracle=read_oracle(argv[4],sha,counts.query_count); const auto budget=std::stoull(argv[5]);
    const std::string profile=argv[6]; Row row; Timing timing;

    if (profile=="B3L") {
      BoundedResident e(graph,ResidentMode::LocalRepair,std::max<std::size_t>(1,graph.vertex_count()),budget);
      timing=run_engine(e,ops,oracle); row.current=e.accounted_algorithm_state_bytes(); row.peak=e.peak_accounted_algorithm_state_bytes();
      row.cold=e.misses(); row.resident=e.hits(); row.evictions=e.evictions();
    } else if (profile=="ADES-V4") {
      ProgressiveConfig c; c.resident_cap=std::max<std::size_t>(1,graph.vertex_count()); c.partial_cap=8; c.persistent_state_budget_bytes=budget;
      ProgressiveADES e(graph,c); timing=run_engine(e,ops,oracle); const auto&s=e.stats();
      if(!s.timing.reconciles()) throw std::logic_error("V4 telemetry mismatch");
      row.current=s.accounted_algorithm_state_bytes; row.peak=s.peak_accounted_algorithm_state_bytes; row.cold=s.cold_queries; row.resident=s.resident_queries;
      row.promotions=s.promotions; row.evictions=s.evictions; row.rebuilds=s.rebuilds; row.memory_rejections=s.memory_budget_rejections;
      row.progressive=s.progressive_queries; row.partial_creations=s.partial_creations; row.partial_invalidations=s.partial_invalidations; row.progressive_promotions=s.progressive_promotions;
    } else if (profile=="ADES-V5") {
      HazardConfig c; c.resident_cap=std::max<std::size_t>(1,graph.vertex_count()); c.partial_cap=8; c.persistent_state_budget_bytes=budget;
      HazardAwareADES e(graph,c); timing=run_engine(e,ops,oracle); const auto&s=e.stats();
      if(!s.timing.reconciles()) throw std::logic_error("V5 telemetry mismatch");
      row.current=s.accounted_algorithm_state_bytes; row.peak=s.peak_accounted_algorithm_state_bytes; row.cold=s.cold_queries; row.resident=s.resident_queries;
      row.promotions=s.promotions; row.evictions=s.evictions; row.rebuilds=s.rebuilds; row.memory_rejections=s.memory_budget_rejections;
      row.progressive=s.progressive_queries; row.partial_creations=s.partial_creations; row.partial_invalidations=s.partial_invalidations; row.progressive_promotions=s.progressive_promotions;
      row.direct_promotions=s.direct_promotions; row.hazard_rejections=s.hazard_rejections; row.direct_fit_rejections=s.direct_fit_rejections;
    } else {
      Config base; base.resident_cap=std::max<std::size_t>(1,graph.vertex_count()); base.persistent_state_budget_bytes=budget;
      ADES e(graph,config_for_scientific_ablation(ablation(profile),base)); timing=run_engine(e,ops,oracle); const auto&s=e.stats();
      if(!s.timing.reconciles()) throw std::logic_error("historical telemetry mismatch");
      row.current=s.accounted_algorithm_state_bytes; row.peak=s.peak_accounted_algorithm_state_bytes; row.cold=s.cold_queries; row.resident=s.resident_queries;
      row.promotions=s.promotions; row.evictions=s.evictions; row.rebuilds=s.rebuilds; row.memory_rejections=s.memory_budget_rejections;
    }
    if(row.peak>budget) throw std::logic_error("PR53 byte-budget violation");
    emit(profile,sha,counts,budget,timing,row); return 0;
  } catch(const std::exception& e) { std::cerr << "ades_pr53_profile: " << e.what() << '\n'; return 2; }
}
