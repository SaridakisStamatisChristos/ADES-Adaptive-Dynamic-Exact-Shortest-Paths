#include "ades/graph.hpp"
#include "ades/trace.hpp"

#include <boost/version.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include <algorithm>
#include <chrono>
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

static_assert(BOOST_VERSION == 109200, "PR59 requires Boost 1.92.0");

using Clock = std::chrono::steady_clock;
using Dist = std::uint64_t;
static constexpr Dist INF = std::numeric_limits<Dist>::max() / 4;

using EdgeProperty = boost::property<
    boost::edge_weight_t, Dist,
    boost::property<boost::edge_index_t, std::uint32_t>>;
using BoostGraph = boost::adjacency_list<boost::vecS, boost::vecS,
                                         boost::directedS,
                                         boost::no_property, EdgeProperty>;

struct OracleBundle {
  std::string trace_sha256;
  std::vector<ades::Distance> answers;
};

static std::uint64_t ns_since(Clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
          .count());
}

static std::uint64_t parse_budget() {
  const char* raw = std::getenv("ADES_PERSISTENT_STATE_BUDGET_BYTES");
  if (!raw || !*raw) return 0;
  char* end = nullptr;
  const auto value = std::strtoull(raw, &end, 10);
  if (!end || *end != '\0') throw std::runtime_error("invalid persistent-state budget");
  return value;
}

static OracleBundle read_oracle_checked(const std::string& path,
                                        const std::vector<ades::TraceOp>& trace) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read oracle: " + path);

  std::string magic, expected_sha;
  std::size_t operation_count = 0, query_count = 0;
  if (!(in >> magic >> operation_count >> query_count >> expected_sha) ||
      magic != "ADES_ORACLE_V2") {
    throw std::runtime_error("bad oracle header");
  }
  if (operation_count != trace.size()) {
    throw std::runtime_error("oracle operation count differs from canonical trace");
  }
  if (expected_sha != ades::trace_sha256(trace)) {
    throw std::runtime_error("oracle trace SHA-256 differs from canonical trace");
  }

  std::vector<ades::Distance> answers;
  answers.reserve(query_count);
  std::size_t query_index = 0;
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
      ++query_index;
    } else {
      throw std::runtime_error("bad oracle operation kind");
    }
  }
  if (query_index != query_count || answers.size() != query_count) {
    throw std::runtime_error("oracle query count mismatch");
  }
  return {expected_sha, std::move(answers)};
}

static std::unique_ptr<BoostGraph> build_boost_graph(const ades::Graph& graph) {
  auto out = std::make_unique<BoostGraph>(graph.vertex_count());
  auto weights = boost::get(boost::edge_weight, *out);
  auto indices = boost::get(boost::edge_index, *out);
  for (std::uint32_t id = 0; id < graph.edge_count(); ++id) {
    const auto& e = graph.edge(id);
    auto [ed, inserted] = boost::add_edge(e.from, e.to, *out);
    if (!inserted) throw std::runtime_error("Boost.Graph edge insertion failed");
    boost::put(weights, ed, e.weight);
    boost::put(indices, ed, id);
  }
  return out;
}

static ades::Distance query_fresh(const BoostGraph& graph, std::uint32_t source,
                                  std::uint32_t target) {
  if (source >= boost::num_vertices(graph) || target >= boost::num_vertices(graph)) {
    throw std::runtime_error("query vertex out of range");
  }
  std::vector<Dist> distances(boost::num_vertices(graph), INF);
  const auto index = boost::get(boost::vertex_index, graph);
  auto distance_map = boost::make_iterator_property_map(distances.begin(), index);
  boost::dijkstra_shortest_paths(
      graph, source,
      boost::distance_map(distance_map)
          .weight_map(boost::get(boost::edge_weight, graph))
          .distance_inf(INF)
          .distance_zero(Dist{0}));
  return distances.at(target);
}

static void update_weight_by_id(BoostGraph& graph, std::uint32_t edge_id,
                                ades::Weight new_weight) {
  auto indices = boost::get(boost::edge_index, graph);
  auto weights = boost::get(boost::edge_weight, graph);
  auto [it, end] = boost::edges(graph);
  for (; it != end; ++it) {
    if (boost::get(indices, *it) == edge_id) {
      boost::put(weights, *it, new_weight);
      return;
    }
  }
  throw std::runtime_error("update edge id not found in Boost.Graph input");
}

static void write_json_string(std::ostream& out, const std::string& value) {
  out << '"';
  for (const char c : value) {
    if (c == '\\' || c == '"') out << '\\';
    out << c;
  }
  out << '"';
}

int main(int argc, char** argv) {
  try {
    if (argc != 5) {
      std::cerr << "usage: boost_graph_direct GRAPH TRACE ORACLE REPORT.json\n";
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
    if (oracle.trace_sha256 != trace_sha || oracle.answers.size() != counts.query_count) {
      throw std::runtime_error("oracle identity/count mismatch");
    }

    const auto preprocessing_start = Clock::now();
    auto graph = build_boost_graph(base);
    const auto preprocessing_ns = ns_since(preprocessing_start);

    std::set<std::uint32_t> unique_sources;
    std::size_t answer_index = 0;
    std::uint64_t query_ns = 0, update_ns = 0;
    bool exact = true;
    std::size_t first_bad_query = std::numeric_limits<std::size_t>::max();
    ades::Distance first_got = 0, first_expected = 0;

    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        const auto start = Clock::now();
        update_weight_by_id(*graph, op.a, op.new_weight);
        update_ns += ns_since(start);
      } else {
        unique_sources.insert(op.a);
        const auto start = Clock::now();
        const auto got = query_fresh(*graph, op.a, op.b);
        query_ns += ns_since(start);
        const auto expected = oracle.answers.at(answer_index);
        if (got != expected && exact) {
          exact = false;
          first_bad_query = answer_index;
          first_got = got;
          first_expected = expected;
        }
        ++answer_index;
      }
    }
    if (answer_index != oracle.answers.size()) {
      throw std::runtime_error("executed query count differs from oracle");
    }

    // PR43 accounting v1 excludes common graph input storage, oracle/harness
    // state, and temporary query workspaces. This recomputation comparator
    // retains no adaptive shortest-path, admission, cache, or policy state.
    constexpr std::uint64_t accounted_algorithm_state_bytes = 0;
    constexpr std::uint64_t peak_accounted_algorithm_state_bytes = 0;
    if (budget != 0 && peak_accounted_algorithm_state_bytes > budget) {
      throw std::runtime_error("persistent-state byte budget exceeded");
    }

    std::ofstream out(report_path, std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write report");
    out << "{\n";
    out << "  \"schema\": \"ades-boost-graph-direct-adapter-v1\",\n";
    out << "  \"boost_version\": \"" << BOOST_LIB_VERSION << "\",\n";
    out << "  \"boost_graph_commit\": \"fc5dac2c552f945846449fd123a9b8335d5c063a\",\n";
    out << "  \"graph_sha256\": "; write_json_string(out, graph_sha); out << ",\n";
    out << "  \"trace_sha256\": "; write_json_string(out, trace_sha); out << ",\n";
    out << "  \"operation_count\": " << trace.size() << ",\n";
    out << "  \"query_count\": " << counts.query_count << ",\n";
    out << "  \"update_count\": " << counts.update_count << ",\n";
    out << "  \"increase_count\": " << counts.increase_count << ",\n";
    out << "  \"decrease_count\": " << counts.decrease_count << ",\n";
    out << "  \"unique_query_sources\": " << unique_sources.size() << ",\n";
    out << "  \"exact_answers\": " << (exact ? "true" : "false") << ",\n";
    out << "  \"oracle_trace_identity\": true,\n";
    out << "  \"online_arrival_order\": true,\n";
    out << "  \"future_knowledge_used\": false,\n";
    out << "  \"graph_load_ns\": " << graph_load_ns << ",\n";
    out << "  \"preprocessing_ns\": " << preprocessing_ns << ",\n";
    out << "  \"query_ns\": " << query_ns << ",\n";
    out << "  \"update_ns\": " << update_ns << ",\n";
    out << "  \"algorithm_ns\": " << (query_ns + update_ns) << ",\n";
    out << "  \"timing_boundary\": \"query/update calls only; oracle comparison, graph load, trace validation, and graph conversion excluded; graph conversion reported as preprocessing_ns\",\n";
    out << "  \"persistent_state_budget_bytes\": " << budget << ",\n";
    out << "  \"accounted_algorithm_state_bytes\": " << accounted_algorithm_state_bytes << ",\n";
    out << "  \"peak_accounted_algorithm_state_bytes\": " << peak_accounted_algorithm_state_bytes << ",\n";
    out << "  \"accounting_version\": 1,\n";
    out << "  \"accounting_boundary\": \"PR43 v1: common graph input, oracle/harness state, and temporary Dijkstra workspaces excluded; no adaptive persistent state retained\",\n";
    out << "  \"direct_status_after_this_run\": \"PENDING\"";
    if (!exact) {
      out << ",\n  \"first_mismatch\": {\"query_index\": " << first_bad_query
          << ", \"got\": " << first_got << ", \"expected\": " << first_expected << "}";
    }
    out << "\n}\n";
    if (!out) throw std::runtime_error("failed while writing report");

    std::cout << "BOOST_DIRECT trace=" << trace_sha
              << " ops=" << trace.size()
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " increases=" << counts.increase_count
              << " decreases=" << counts.decrease_count
              << " sources=" << unique_sources.size()
              << " exact=" << (exact ? 1 : 0)
              << " algorithm_ns=" << (query_ns + update_ns)
              << " preprocessing_ns=" << preprocessing_ns
              << " accounted_bytes=0\n";
    return exact ? 0 : 3;
  } catch (const std::exception& error) {
    std::cerr << "boost_graph_direct: " << error.what() << '\n';
    return 2;
  }
}
