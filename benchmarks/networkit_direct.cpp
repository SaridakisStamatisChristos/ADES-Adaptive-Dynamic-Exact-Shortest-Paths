#include "ades/graph.hpp"
#include "ades/trace.hpp"

#include <networkit/auxiliary/Multiprecision.hpp>
#include <networkit/distance/DynDijkstra.hpp>
#include <networkit/dynamics/GraphEvent.hpp>
#include <networkit/graph/Graph.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;
static constexpr const char* kNetworKitCommit =
    "6b3878ee00a6eadd50be3b3047fbcae56bcac685";

struct OracleBundle {
  std::string trace_sha256;
  std::vector<ades::Distance> answers;
};

struct LogicalEdge {
  std::uint32_t u = 0;
  std::uint32_t v = 0;
  ades::Weight weight = 0;
  std::uint32_t arc_uv = 0;
  std::uint32_t arc_vu = 0;
};

struct CacheEntry {
  std::uint32_t source = 0;
  std::uint64_t last_use = 0;
  std::unique_ptr<NetworKit::DynDijkstra> sssp;
};

static std::uint64_t ns_since(Clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
          .count());
}

static std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
  if (b > std::numeric_limits<std::uint64_t>::max() - a)
    throw std::overflow_error("persistent-state accounting overflow");
  return a + b;
}

static std::uint64_t checked_mul(std::uint64_t a, std::uint64_t b) {
  if (a && b > std::numeric_limits<std::uint64_t>::max() / a)
    throw std::overflow_error("persistent-state accounting overflow");
  return a * b;
}

static std::uint64_t parse_budget() {
  const char* raw = std::getenv("ADES_PERSISTENT_STATE_BUDGET_BYTES");
  if (!raw || !*raw)
    throw std::runtime_error("NetworKit DIRECT adapter requires an explicit persistent-state budget");
  char* end = nullptr;
  const auto value = std::strtoull(raw, &end, 10);
  if (!end || *end != '\0' || value == 0)
    throw std::runtime_error("invalid persistent-state budget");
  return value;
}

static OracleBundle read_oracle_checked(
    const std::string& path, const std::vector<ades::TraceOp>& trace) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read oracle: " + path);

  std::string magic, expected_sha;
  std::size_t operation_count = 0, query_count = 0;
  if (!(in >> magic >> operation_count >> query_count >> expected_sha) ||
      magic != "ADES_ORACLE_V2") {
    throw std::runtime_error("bad oracle header");
  }
  if (operation_count != trace.size())
    throw std::runtime_error("oracle operation count differs from canonical trace");
  if (expected_sha != ades::trace_sha256(trace))
    throw std::runtime_error("oracle trace SHA-256 differs from canonical trace");

  std::vector<ades::Distance> answers;
  answers.reserve(query_count);
  for (std::size_t i = 0; i < operation_count; ++i) {
    std::string kind;
    if (!(in >> kind)) throw std::runtime_error("truncated oracle");
    const auto& op = trace.at(i);
    if (kind == "UPDATE") {
      std::uint32_t edge_id = 0;
      ades::Weight old_weight = 0, new_weight = 0;
      if (!(in >> edge_id >> old_weight >> new_weight) ||
          op.kind != ades::TraceOpKind::Update || op.a != edge_id ||
          op.old_weight != old_weight || op.new_weight != new_weight) {
        throw std::runtime_error("oracle UPDATE differs from canonical trace");
      }
    } else if (kind == "QUERY") {
      std::uint32_t source = 0, target = 0;
      ades::Distance answer = 0;
      if (!(in >> source >> target >> answer) ||
          op.kind != ades::TraceOpKind::Query || op.a != source || op.b != target) {
        throw std::runtime_error("oracle QUERY differs from canonical trace");
      }
      answers.push_back(answer);
    } else {
      throw std::runtime_error("bad oracle operation kind");
    }
  }
  if (answers.size() != query_count)
    throw std::runtime_error("oracle query count mismatch");
  return {expected_sha, std::move(answers)};
}

static std::vector<LogicalEdge> validate_symmetric_graph(const ades::Graph& graph) {
  if (graph.edge_count() % 2 != 0)
    throw std::runtime_error("PR68 symmetric graph requires an even directed-arc count");
  std::vector<LogicalEdge> logical;
  logical.reserve(graph.edge_count() / 2);
  for (std::uint32_t id = 0; id < graph.edge_count(); id += 2) {
    const auto& a = graph.edge(id);
    const auto& b = graph.edge(id + 1);
    if (a.from == a.to || b.from == b.to)
      throw std::runtime_error("PR68 graph must not contain self loops");
    if (a.from != b.to || a.to != b.from || a.weight != b.weight)
      throw std::runtime_error("PR68 directed arcs are not exact symmetric pairs");
    logical.push_back({a.from, a.to, a.weight, id, static_cast<std::uint32_t>(id + 1)});
  }
  return logical;
}

static NetworKit::Graph build_networkit_graph(std::size_t vertices,
                                              const std::vector<LogicalEdge>& edges) {
  NetworKit::Graph out(vertices, true, false, false);
  for (const auto& e : edges) {
    if (out.hasEdge(e.u, e.v))
      throw std::runtime_error("PR68 logical graph must be simple");
    out.addEdge(e.u, e.v, static_cast<NetworKit::edgeweight>(e.weight));
  }
  if (out.numberOfEdges() != edges.size())
    throw std::runtime_error("NetworKit logical edge-count mismatch");
  return out;
}

static std::uint64_t per_source_accounted_bytes(std::size_t vertices) {
  // PR43/PR54-style logical persistent algorithm state.  The shared graph input
  // is excluded for both implementations.  DynDijkstra persists distances,
  // path-count state, color state, and update-distance state per source.  Empty
  // reusable heap capacity is treated as non-semantic scratch, matching the
  // existing accounting boundary; allocator/container overhead is excluded.
  const std::uint64_t per_vertex =
      sizeof(NetworKit::edgeweight) + sizeof(NetworKit::bigfloat) +
      sizeof(std::uint32_t) + sizeof(NetworKit::edgeweight);
  auto bytes = checked_mul(static_cast<std::uint64_t>(vertices), per_vertex);
  bytes = checked_add(bytes, sizeof(std::uint32_t) + sizeof(std::uint64_t));
  return bytes;
}

static ades::Distance exact_integer_distance(double value) {
  if (!std::isfinite(value) || value < 0.0 || value > 9007199254740992.0)
    throw std::runtime_error("NetworKit returned non-finite/out-of-exact-integer-range distance");
  const auto out = static_cast<ades::Distance>(value);
  if (static_cast<double>(out) != value)
    throw std::runtime_error("NetworKit returned non-integral distance under integer-weight contract");
  return out;
}

static void write_json_string(std::ostream& out, const std::string& value) {
  out << '"';
  for (char c : value) {
    if (c == '\\' || c == '"') out << '\\';
    out << c;
  }
  out << '"';
}

int main(int argc, char** argv) {
  try {
    if (argc != 5) {
      std::cerr << "usage: networkit_direct GRAPH TRACE ORACLE REPORT.json\n";
      return 2;
    }
    const std::string graph_path = argv[1];
    const std::string trace_path = argv[2];
    const std::string oracle_path = argv[3];
    const std::string report_path = argv[4];
    const auto budget = parse_budget();

    const auto load_start = Clock::now();
    ades::Graph base = ades::Graph::load_dimacs_gr_gz(graph_path);
    const auto graph_load_ns = ns_since(load_start);
    const auto graph_sha = ades::file_sha256(graph_path);

    const auto trace = ades::read_trace(trace_path);
    ades::validate_trace_against_graph(base, trace);
    const auto trace_sha = ades::trace_sha256(trace);
    const auto counts = ades::trace_counts(trace);
    const auto oracle = read_oracle_checked(oracle_path, trace);
    if (oracle.trace_sha256 != trace_sha || oracle.answers.size() != counts.query_count)
      throw std::runtime_error("oracle identity/count mismatch");

    const auto preprocessing_start = Clock::now();
    auto logical = validate_symmetric_graph(base);
    auto graph = build_networkit_graph(base.vertex_count(), logical);
    const auto per_source_bytes = per_source_accounted_bytes(base.vertex_count());
    const auto preprocessing_ns = ns_since(preprocessing_start);

    std::vector<CacheEntry> cache;
    std::set<std::uint32_t> unique_sources;
    std::uint64_t logical_clock = 0;
    std::uint64_t current_bytes = 0, peak_bytes = 0;
    std::uint64_t query_ns = 0, update_ns = 0;
    std::uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0;
    std::size_t logical_update_count = 0;
    std::size_t logical_increase_count = 0, logical_decrease_count = 0;
    std::size_t answer_index = 0;
    bool exact = true;
    std::size_t first_bad_query = std::numeric_limits<std::size_t>::max();
    ades::Distance first_got = 0, first_expected = 0;

    for (std::size_t i = 0; i < trace.size();) {
      const auto& op = trace[i];
      if (op.kind == ades::TraceOpKind::Query) {
        unique_sources.insert(op.a);
        ++logical_clock;
        const auto start = Clock::now();
        ades::Distance got = 0;
        auto it = std::find_if(cache.begin(), cache.end(),
                               [&](const CacheEntry& e) { return e.source == op.a; });
        if (it != cache.end()) {
          ++cache_hits;
          it->last_use = logical_clock;
          got = exact_integer_distance(it->sssp->distance(op.b));
        } else {
          ++cache_misses;
          while (!cache.empty() && current_bytes + per_source_bytes > budget) {
            auto victim = std::min_element(cache.begin(), cache.end(),
                                           [](const CacheEntry& a, const CacheEntry& b) {
                                             return a.last_use < b.last_use;
                                           });
            cache.erase(victim);
            current_bytes -= per_source_bytes;
            ++cache_evictions;
          }
          auto sssp = std::make_unique<NetworKit::DynDijkstra>(graph, op.a, false);
          sssp->run();
          got = exact_integer_distance(sssp->distance(op.b));
          if (per_source_bytes <= budget && current_bytes + per_source_bytes <= budget) {
            cache.push_back({op.a, logical_clock, std::move(sssp)});
            current_bytes += per_source_bytes;
            peak_bytes = std::max(peak_bytes, current_bytes);
          }
        }
        query_ns += ns_since(start);
        const auto expected = oracle.answers.at(answer_index);
        if (got != expected && exact) {
          exact = false;
          first_bad_query = answer_index;
          first_got = got;
          first_expected = expected;
        }
        ++answer_index;
        ++i;
        continue;
      }

      // One logical undirected replacement must be exactly two adjacent ADES
      // symmetric-arc UPDATE records.
      if (i + 1 >= trace.size() || trace[i + 1].kind != ades::TraceOpKind::Update)
        throw std::runtime_error("unpaired PR68 symmetric UPDATE");
      const auto& first = trace[i];
      const auto& second = trace[i + 1];
      if ((first.a & 1U) != 0U || second.a != first.a + 1 ||
          first.old_weight != second.old_weight ||
          first.new_weight != second.new_weight) {
        throw std::runtime_error("non-canonical PR68 symmetric UPDATE pair");
      }
      const auto logical_id = first.a / 2U;
      if (logical_id >= logical.size())
        throw std::runtime_error("logical edge id out of range");
      auto& e = logical[logical_id];
      if (e.arc_uv != first.a || e.arc_vu != second.a ||
          e.weight != first.old_weight) {
        throw std::runtime_error("logical edge/trace state mismatch");
      }

      const auto start = Clock::now();
      graph.removeEdge(e.u, e.v);
      const NetworKit::GraphEvent removal(NetworKit::GraphEvent::EDGE_REMOVAL,
                                          e.u, e.v,
                                          static_cast<double>(e.weight));
      for (auto& entry : cache) entry.sssp->update(removal);

      graph.addEdge(e.u, e.v, static_cast<double>(first.new_weight));
      const NetworKit::GraphEvent addition(NetworKit::GraphEvent::EDGE_ADDITION,
                                           e.u, e.v,
                                           static_cast<double>(first.new_weight));
      for (auto& entry : cache) entry.sssp->update(addition);
      update_ns += ns_since(start);

      base.update_weight(first.a, first.new_weight);
      base.update_weight(second.a, second.new_weight);
      if (first.new_weight > first.old_weight)
        ++logical_increase_count;
      else if (first.new_weight < first.old_weight)
        ++logical_decrease_count;
      else
        throw std::runtime_error("PR68 logical update must change weight");
      e.weight = first.new_weight;
      ++logical_update_count;
      i += 2;
    }

    if (answer_index != oracle.answers.size())
      throw std::runtime_error("executed query count differs from oracle");
    if (peak_bytes > budget)
      throw std::runtime_error("persistent-state byte budget exceeded");
    if (logical_update_count * 2 != counts.update_count)
      throw std::runtime_error("raw/logical update-count parity failure");

    const auto algorithm_ns = query_ns + update_ns;
    std::ofstream out(report_path, std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write report");
    out << "{\n";
    out << "  \"schema\": \"ades-networkit-direct-adapter-v1\",\n";
    out << "  \"networkit_version\": \"11.2.2\",\n";
    out << "  \"networkit_commit\": \"" << kNetworKitCommit << "\",\n";
    out << "  \"upstream_algorithm_modified\": false,\n";
    out << "  \"graph_sha256\": "; write_json_string(out, graph_sha); out << ",\n";
    out << "  \"trace_sha256\": "; write_json_string(out, trace_sha); out << ",\n";
    out << "  \"operation_count\": " << trace.size() << ",\n";
    out << "  \"query_count\": " << counts.query_count << ",\n";
    out << "  \"update_count\": " << counts.update_count << ",\n";
    out << "  \"increase_count\": " << counts.increase_count << ",\n";
    out << "  \"decrease_count\": " << counts.decrease_count << ",\n";
    out << "  \"logical_update_count\": " << logical_update_count << ",\n";
    out << "  \"logical_increase_count\": " << logical_increase_count << ",\n";
    out << "  \"logical_decrease_count\": " << logical_decrease_count << ",\n";
    out << "  \"unique_query_sources\": " << unique_sources.size() << ",\n";
    out << "  \"exact_answers\": " << (exact ? "true" : "false") << ",\n";
    out << "  \"oracle_trace_identity\": true,\n";
    out << "  \"online_arrival_order\": true,\n";
    out << "  \"future_knowledge_used\": false,\n";
    out << "  \"symmetric_arc_contract\": true,\n";
    out << "  \"graph_load_ns\": " << graph_load_ns << ",\n";
    out << "  \"preprocessing_ns\": " << preprocessing_ns << ",\n";
    out << "  \"query_ns\": " << query_ns << ",\n";
    out << "  \"update_ns\": " << update_ns << ",\n";
    out << "  \"algorithm_ns\": " << algorithm_ns << ",\n";
    out << "  \"timing_boundary\": \"query includes source-cache lookup and DynDijkstra construction/run on miss; update includes shared-graph remove/add plus DynDijkstra remove/add propagation to every retained source; oracle comparison and graph/trace load excluded\",\n";
    out << "  \"persistent_state_budget_bytes\": " << budget << ",\n";
    out << "  \"per_retained_source_accounted_bytes\": " << per_source_bytes << ",\n";
    out << "  \"accounted_algorithm_state_bytes\": " << current_bytes << ",\n";
    out << "  \"peak_accounted_algorithm_state_bytes\": " << peak_bytes << ",\n";
    out << "  \"accounting_version\": 2,\n";
    out << "  \"accounting_boundary\": \"PR43/PR54-style logical persistent algorithm state: per-source DynDijkstra distances, bigfloat path-count state, color state, and update-distance state plus LRU metadata; common shared graph input, reusable empty heap scratch, temporaries, allocator/container overhead and telemetry excluded; RSS reported separately\",\n";
    out << "  \"cache_hits\": " << cache_hits << ",\n";
    out << "  \"cache_misses\": " << cache_misses << ",\n";
    out << "  \"cache_evictions\": " << cache_evictions;
    if (!exact) {
      out << ",\n  \"first_mismatch\": {\"query_index\": " << first_bad_query
          << ", \"got\": " << first_got << ", \"expected\": "
          << first_expected << "}";
    }
    out << "\n}\n";
    if (!out) throw std::runtime_error("failed while writing report");

    std::cout << "NETWORKIT_DIRECT trace=" << trace_sha
              << " queries=" << counts.query_count
              << " raw_updates=" << counts.update_count
              << " logical_updates=" << logical_update_count
              << " sources=" << unique_sources.size()
              << " exact=" << (exact ? 1 : 0)
              << " algorithm_ns=" << algorithm_ns
              << " query_ns=" << query_ns
              << " update_ns=" << update_ns
              << " cache_hits=" << cache_hits
              << " cache_misses=" << cache_misses
              << " evictions=" << cache_evictions
              << " per_source_bytes=" << per_source_bytes
              << " peak_bytes=" << peak_bytes << '\n';
    return exact ? 0 : 3;
  } catch (const std::exception& error) {
    std::cerr << "networkit_direct: " << error.what() << '\n';
    return 2;
  }
}
