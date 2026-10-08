#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/bounded_baselines.hpp"
#include "ades/memory_budget.hpp"
#include "ades/telemetry.hpp"
#include "ades/trace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace ades;
using Clock = std::chrono::steady_clock;

struct Op {
  bool update;
  std::uint32_t a, b;
  Weight w;
  Weight old_w;
};

struct OracleResult {
  std::vector<Distance> answers;
  std::uint64_t time_ns = 0;
};

struct RunTiming {
  std::uint64_t algorithm_ns = 0;
  std::uint64_t query_ns = 0;
  std::uint64_t update_ns = 0;
  std::vector<std::uint64_t> operation_latencies;
  std::vector<std::uint64_t> query_latencies;
  std::vector<std::uint64_t> update_latencies;
};

static std::uint64_t elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

static std::uint64_t percentile_ns(std::vector<std::uint64_t> values, double p) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(values.size())));
  const auto index = std::min(values.size() - 1, rank ? rank - 1 : std::size_t{0});
  return values[index];
}

static std::vector<std::uint32_t> hot_pool(std::size_t n, std::size_t k,
                                           std::mt19937_64& r) {
  std::vector<std::uint32_t> v(n);
  for (std::uint32_t i = 0; i < n; i++) v[i] = i;
  std::shuffle(v.begin(), v.end(), r);
  v.resize(std::min(k, n));
  return v;
}

static std::uint32_t pick_source(const std::string& family,
                                 const std::vector<std::uint32_t>& hot,
                                 std::size_t q, std::size_t epoch,
                                 std::size_t n, std::mt19937_64& r) {
  if (family == "uniform") return r() % n;
  if (family == "single-hot") return (r() % 100 < 95) ? hot[0] : r() % n;
  if (family == "rotating-hot")
    return (r() % 100 < 90) ? hot[(q / epoch) % hot.size()] : r() % n;
  if (family == "hot-pool")
    return (r() % 100 < 90) ? hot[r() % hot.size()] : r() % n;
  if (family == "churn") return hot[(q / epoch) % hot.size()];
  if (family == "zipf") {
    double u = std::generate_canonical<double, 53>(r), h = 0;
    for (std::size_t i = 1; i <= hot.size(); ++i) h += 1.0 / double(i);
    double c = 0;
    for (std::size_t i = 1; i <= hot.size(); ++i) {
      c += (1.0 / double(i)) / h;
      if (u <= c) return hot[i - 1];
    }
    return hot.back();
  }
  throw std::runtime_error("unknown source family");
}

static std::uint32_t pick_directional_edge(const Graph& g,
                                           std::uint32_t start,
                                           bool increase) {
  if (!g.edge_count()) throw std::runtime_error("cannot update an empty graph");
  constexpr Weight max_w = std::numeric_limits<Weight>::max();
  for (std::size_t i = 0; i < g.edge_count(); ++i) {
    auto id = static_cast<std::uint32_t>((static_cast<std::size_t>(start) + i) % g.edge_count());
    const auto w = g.edge(id).weight;
    if (increase ? (w <= max_w - 31) : (w > 0)) return id;
  }
  throw std::runtime_error(increase ? "no edge can be safely increased"
                                    : "no positive-weight edge can be decreased");
}

static std::vector<Op> make_trace(const Graph& g, const std::string& family,
                                  std::uint64_t seed, std::size_t queries,
                                  std::size_t update_every,
                                  std::size_t hot_sources,
                                  std::size_t epoch,
                                  const std::string& update_mode) {
  Graph trace_g = g;
  std::mt19937_64 r(seed);
  auto hot = hot_pool(trace_g.vertex_count(), std::max<std::size_t>(1, hot_sources), r);
  std::vector<Op> ops;
  ops.reserve(queries + (update_every ? queries / update_every : 0));
  std::size_t update_index = 0;
  for (std::size_t q = 0; q < queries; q++) {
    if (update_every && q && q % update_every == 0) {
      auto id = static_cast<std::uint32_t>(r() % trace_g.edge_count());
      auto old = trace_g.edge(id).weight;
      Weight nw = old;
      if (update_mode == "random") {
        nw = (r() & 1) ? old + 1 + (r() % 31)
                       : (old ? old - std::min<Weight>(old, r() % std::min<Weight>(old + 1, 31)) : 0);
      } else if (update_mode == "alternating") {
        const bool increase = (update_index % 2 == 0);
        id = pick_directional_edge(trace_g, id, increase);
        old = trace_g.edge(id).weight;
        if (increase) nw = old + 1 + (r() % 31);
        else {
          const Weight bound = std::min<Weight>(old, 31);
          nw = old - (1 + (r() % bound));
        }
      } else {
        throw std::runtime_error("unknown update mode");
      }
      ++update_index;
      trace_g.update_weight(id, nw);
      ops.push_back({true, id, 0, nw, old});
    }
    auto s = pick_source(family, hot, q, std::max<std::size_t>(1, epoch), g.vertex_count(), r);
    auto t = static_cast<std::uint32_t>(r() % g.vertex_count());
    ops.push_back({false, s, t, 0, 0});
  }
  return ops;
}

static std::vector<TraceOp> canonicalize(const std::vector<Op>& ops) {
  std::vector<TraceOp> trace;
  trace.reserve(ops.size());
  for (const auto& op : ops)
    trace.push_back(op.update ? TraceOp::update(op.a, op.old_w, op.w)
                              : TraceOp::query(op.a, op.b));
  return trace;
}

static OracleResult oracle(Graph g, const std::vector<Op>& ops) {
  OracleResult result;
  result.answers.reserve(ops.size());
  const auto start = Clock::now();
  for (const auto& o : ops) {
    if (o.update) g.update_weight(o.a, o.w);
    else result.answers.push_back(dijkstra(g, o.a).dist[o.b]);
  }
  result.time_ns = elapsed_ns(start);
  return result;
}

template <class E>
static RunTiming run(E& e, const std::vector<Op>& ops,
                     const std::vector<Distance>& ref) {
  RunTiming timing;
  timing.operation_latencies.reserve(ops.size());
  timing.query_latencies.reserve(ref.size());
  timing.update_latencies.reserve(ops.size() - ref.size());
  std::size_t qi = 0;
  for (const auto& o : ops) {
    const auto start = Clock::now();
    if (o.update) {
      e.update(o.a, o.w);
      const auto ns = elapsed_ns(start);
      timing.update_ns += ns;
      timing.update_latencies.push_back(ns);
      timing.operation_latencies.push_back(ns);
    } else {
      const auto answer = e.query(o.a, o.b);
      const auto ns = elapsed_ns(start);
      timing.query_ns += ns;
      timing.query_latencies.push_back(ns);
      timing.operation_latencies.push_back(ns);
      if (answer != ref[qi++]) {
        std::cerr << "exactness failure\n";
        std::exit(3);
      }
    }
  }
  timing.algorithm_ns = timing.query_ns + timing.update_ns;
  return timing;
}

struct B1 {
  Graph g;
  Distance query(std::uint32_t s, std::uint32_t t) { return bidirectional_dijkstra(g, s, t); }
  void update(std::uint32_t e, Weight w) { g.update_weight(e, w); }
};

static std::uint64_t persistent_budget_from_env() {
  const char* raw = std::getenv("ADES_PERSISTENT_STATE_BUDGET_BYTES");
  if (!raw || !*raw) return 0;
  std::string value(raw);
  std::size_t pos = 0;
  std::uint64_t budget = 0;
  try { budget = std::stoull(value, &pos); }
  catch (const std::exception&) { throw std::runtime_error("invalid ADES_PERSISTENT_STATE_BUDGET_BYTES"); }
  if (pos != value.size() || !budget)
    throw std::runtime_error("invalid ADES_PERSISTENT_STATE_BUDGET_BYTES");
  return budget;
}

int main(int argc, char** argv) {
  try {
    if (argc < 9) {
      std::cerr << "usage: ades_phase graph B1|B2|B3|B4|B2L|B3L|ALL family queries update_every hot_sources epoch seed [cap] [random|alternating]\n"
                   "optional env: ADES_PERSISTENT_STATE_BUDGET_BYTES=N\n";
      return 2;
    }

    auto g = Graph::load_dimacs_gr_gz(argv[1]);
    std::string base = argv[2], family = argv[3];
    auto nq = std::strtoull(argv[4], nullptr, 10);
    auto ue = std::strtoull(argv[5], nullptr, 10);
    auto hs = std::strtoull(argv[6], nullptr, 10);
    auto ep = std::strtoull(argv[7], nullptr, 10);
    std::uint64_t seed = std::strtoull(argv[8], nullptr, 10);
    std::size_t cap = argc > 9 ? std::strtoull(argv[9], nullptr, 10) : 8;
    std::string update_mode = argc > 10 ? argv[10] : "random";
    const auto persistent_budget = persistent_budget_from_env();

    if (persistent_budget && (base == "B2" || base == "B3" || base == "ALL")) {
      std::cerr << "persistent byte-budget mode is supported by B1, B2L, B3L and B4; unbounded B2/B3 are not admissible\n";
      return 2;
    }
    if ((base == "B2L" || base == "B3L") && cap == 0 && !persistent_budget) {
      std::cerr << "bounded comparator requires cap > 0\n";
      return 2;
    }
    if (update_mode != "random" && update_mode != "alternating") {
      std::cerr << "update mode must be random or alternating\n";
      return 2;
    }

    std::size_t effective_cap = cap;
    if (persistent_budget && (base == "B2L" || base == "B3L" || base == "B4"))
      effective_cap = std::max<std::size_t>(1, g.vertex_count());

    auto ops = make_trace(g, family, seed, nq, ue, hs, ep, update_mode);
    const auto canonical = canonicalize(ops);
    validate_trace_against_graph(g, canonical);
    const auto sha = trace_sha256(canonical);
    const auto counts = trace_counts(canonical);
    const auto unchanged = counts.update_count - counts.increase_count - counts.decrease_count;
    std::cerr << "TRACE_IDENTITY sha256=" << sha
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " increases=" << counts.increase_count
              << " decreases=" << counts.decrease_count
              << " unchanged=" << unchanged
              << " mode=" << update_mode << "\n";

    std::unordered_set<std::uint32_t> distinct;
    for (const auto& op : ops) if (!op.update) distinct.insert(op.a);
    const std::uint64_t per_vertex = sizeof(Distance) + 4 * sizeof(std::int64_t) + sizeof(std::uint32_t);
    const auto n = static_cast<std::uint64_t>(g.vertex_count());
    const auto sources = static_cast<std::uint64_t>(distinct.size());
    const bool overflow = n && (sources > std::numeric_limits<std::uint64_t>::max() / n ||
                                (sources * n) > std::numeric_limits<std::uint64_t>::max() / per_vertex);
    const std::uint64_t resident_bytes = overflow ? std::numeric_limits<std::uint64_t>::max()
                                                  : sources * n * per_vertex;
    std::cerr << "MEMORY_PREFLIGHT baseline=" << base << " vertices=" << n
              << " distinct_sources=" << sources
              << " resident_state_payload_bytes=" << resident_bytes
              << " estimate_type=lower_bound_excludes_graph_containers_allocator_and_temporaries\n";

    if (const char* raw = std::getenv("ADES_MAX_RESIDENT_BYTES");
        raw && *raw && (base == "B2" || base == "B3" || base == "ALL")) {
      std::string limit(raw); std::size_t pos = 0; std::uint64_t budget = 0;
      try { budget = std::stoull(limit, &pos); }
      catch (const std::exception&) { std::cerr << "invalid ADES_MAX_RESIDENT_BYTES\n"; return 2; }
      if (pos != limit.size() || !budget) { std::cerr << "invalid ADES_MAX_RESIDENT_BYTES\n"; return 2; }
      std::cerr << "MEMORY_PREFLIGHT budget_bytes=" << budget << "\n";
      if (overflow || resident_bytes > budget) {
        std::cerr << "MEMORY_PREFLIGHT REJECT: resident payload lower bound exceeds budget; no baseline executed\n";
        return 4;
      }
    }

    const auto oracle_result = oracle(g, ops);
    const auto& ref = oracle_result.answers;

    auto emit = [&](const std::string& b, const RunTiming& rt, const Stats& st) {
      const auto& it = st.timing;
      std::cout << kPublicationTelemetrySchemaVersion << ',' << b << ',' << family << ',' << seed << ',' << sha << ','
                << counts.query_count << ',' << counts.update_count << ','
                << counts.increase_count << ',' << counts.decrease_count << ','
                << ue << ',' << hs << ',' << ep << ',' << effective_cap << ','
                << st.persistent_state_budget_bytes << ','
                << st.accounted_algorithm_state_bytes << ','
                << st.peak_accounted_algorithm_state_bytes << ','
                << kPersistentStateAccountingVersion << ','
                << oracle_result.time_ns << ',' << rt.algorithm_ns << ',' << rt.query_ns << ',' << rt.update_ns << ','
                << percentile_ns(rt.operation_latencies,0.50) << ',' << percentile_ns(rt.operation_latencies,0.95) << ','
                << percentile_ns(rt.query_latencies,0.50) << ',' << percentile_ns(rt.query_latencies,0.95) << ','
                << percentile_ns(rt.update_latencies,0.50) << ',' << percentile_ns(rt.update_latencies,0.95) << ','
                << st.cold_queries << ',' << st.resident_queries << ',' << st.promotions << ',' << st.evictions << ','
                << st.rebuilds << ',' << st.repair_aborts << ',' << st.cooldown_blocks << ',' << st.admission_rejections << ','
                << st.filtered_updates << ',' << st.decrease_repairs << ',' << st.increase_repairs << ','
                << st.memory_budget_rejections << ',' << st.memory_metadata_prunes << ','
                << it.query_time_ns << ',' << it.update_time_ns << ','
                << it.cold_query_ns << ',' << it.resident_query_ns << ',' << it.query_policy_ns << ','
                << it.promotion_ns << ',' << it.eviction_ns << ',' << it.graph_update_ns << ','
                << it.decrease_repair_ns << ',' << it.increase_repair_ns << ',' << it.rebuild_ns << ','
                << it.controller_ns << ',' << it.update_policy_ns << '\n';
    };

    auto one = [&](const std::string& b) {
      RunTiming rt{}; Stats st{};
      if (b == "B1") {
        B1 e{g}; rt = run(e, ops, ref); st.persistent_state_budget_bytes = persistent_budget;
      } else if (b == "B2") {
        AlwaysResident e(g, ResidentMode::FullRebuild); rt = run(e, ops, ref);
      } else if (b == "B3") {
        AlwaysResident e(g, ResidentMode::LocalRepair); rt = run(e, ops, ref);
      } else if (b == "B2L" || b == "B3L") {
        BoundedResident e(g, b == "B2L" ? ResidentMode::FullRebuild : ResidentMode::LocalRepair,
                          effective_cap, persistent_budget);
        rt = run(e, ops, ref);
        st.cold_queries = e.misses(); st.resident_queries = e.hits(); st.evictions = e.evictions();
        st.persistent_state_budget_bytes = e.persistent_state_budget_bytes();
        st.accounted_algorithm_state_bytes = e.accounted_algorithm_state_bytes();
        st.peak_accounted_algorithm_state_bytes = e.peak_accounted_algorithm_state_bytes();
        std::cerr << "BOUNDED_RESIDENCY baseline=" << b << " cap_sources=" << effective_cap
                  << " budget_bytes=" << e.persistent_state_budget_bytes()
                  << " accounted_bytes=" << e.accounted_algorithm_state_bytes()
                  << " peak_accounted_bytes=" << e.peak_accounted_algorithm_state_bytes()
                  << " accounting_version=" << kPersistentStateAccountingVersion
                  << " final_resident=" << e.resident_count()
                  << " misses=" << e.misses() << " hits=" << e.hits()
                  << " evictions=" << e.evictions() << "\n";
      } else if (b == "B4") {
        Config cfg; cfg.resident_cap = effective_cap; cfg.persistent_state_budget_bytes = persistent_budget;
        ADES e(g, cfg); rt = run(e, ops, ref); st = e.stats();
        if (!st.timing.reconciles()) {
          std::cerr << "B4 telemetry reconciliation failure\n";
          return false;
        }
        std::cerr << "ADES_MEMORY budget_bytes=" << st.persistent_state_budget_bytes
                  << " accounted_bytes=" << st.accounted_algorithm_state_bytes
                  << " peak_accounted_bytes=" << st.peak_accounted_algorithm_state_bytes
                  << " accounting_version=" << kPersistentStateAccountingVersion
                  << " budget_rejections=" << st.memory_budget_rejections
                  << " metadata_prunes=" << st.memory_metadata_prunes << "\n";
      } else return false;

      if (rt.algorithm_ns != rt.query_ns + rt.update_ns) {
        std::cerr << "top-level telemetry reconciliation failure\n";
        return false;
      }
      if (persistent_budget && st.peak_accounted_algorithm_state_bytes > persistent_budget) {
        std::cerr << "persistent-state budget violation\n";
        return false;
      }
      emit(b, rt, st);
      return true;
    };

    if (base == "ALL") {
      for (const char* b : {"B1", "B2", "B3", "B4"}) if (!one(b)) return 2;
    } else if (!one(base)) return 2;
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ades_phase: " << error.what() << '\n';
    return 2;
  }
}
