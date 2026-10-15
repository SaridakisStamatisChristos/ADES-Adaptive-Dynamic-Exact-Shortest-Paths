#include "ades/baselines.hpp"
#include "ades/graph.hpp"
#include "ades/trace.hpp"

#include <boost/version.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(BOOST_VERSION == 109200, "PR62 oracle requires Boost 1.92.0");

using Dist = std::uint64_t;
static constexpr Dist BOOST_INF = std::numeric_limits<Dist>::max() / 4;
using EdgeProperty = boost::property<
    boost::edge_weight_t, Dist,
    boost::property<boost::edge_index_t, std::uint32_t>>;
using BoostGraph = boost::adjacency_list<boost::vecS, boost::vecS,
                                         boost::directedS,
                                         boost::no_property, EdgeProperty>;
using EdgeDescriptor = boost::graph_traits<BoostGraph>::edge_descriptor;

struct BoostBundle {
  std::unique_ptr<BoostGraph> graph;
  std::vector<EdgeDescriptor> edge_by_id;
};

static BoostBundle build_boost_graph(const ades::Graph& graph) {
  auto out = std::make_unique<BoostGraph>(graph.vertex_count());
  std::vector<EdgeDescriptor> by_id(graph.edge_count());
  auto weights = boost::get(boost::edge_weight, *out);
  auto indices = boost::get(boost::edge_index, *out);
  for (std::uint32_t id = 0; id < graph.edge_count(); ++id) {
    const auto& e = graph.edge(id);
    auto [ed, inserted] = boost::add_edge(e.from, e.to, *out);
    if (!inserted) throw std::runtime_error("Boost.Graph oracle edge insertion failed");
    boost::put(weights, ed, e.weight);
    boost::put(indices, ed, id);
    by_id[id] = ed;
  }
  return {std::move(out), std::move(by_id)};
}

static ades::Distance boost_query(const BoostGraph& graph,
                                  std::uint32_t source,
                                  std::uint32_t target) {
  std::vector<Dist> distances(boost::num_vertices(graph), BOOST_INF);
  const auto index = boost::get(boost::vertex_index, graph);
  auto distance_map = boost::make_iterator_property_map(distances.begin(), index);
  boost::dijkstra_shortest_paths(
      graph, source,
      boost::distance_map(distance_map)
          .weight_map(boost::get(boost::edge_weight, graph))
          .distance_inf(BOOST_INF)
          .distance_zero(Dist{0}));
  return distances.at(target);
}

static void boost_update(BoostBundle& bundle, std::uint32_t edge_id,
                         ades::Weight new_weight) {
  if (edge_id >= bundle.edge_by_id.size()) {
    throw std::runtime_error("Boost oracle update edge id out of range");
  }
  auto weights = boost::get(boost::edge_weight, *bundle.graph);
  boost::put(weights, bundle.edge_by_id[edge_id], new_weight);
}

int main(int argc, char** argv) {
  try {
    if (argc != 4) {
      std::cerr << "usage: oracle_from_trace GRAPH TRACE ORACLE_OUT\n";
      return 2;
    }
    const std::string graph_path = argv[1];
    const std::string trace_path = argv[2];
    const std::string oracle_path = argv[3];

    auto ades_graph = ades::Graph::load_dimacs_gr_gz(graph_path);
    const auto trace = ades::read_trace(trace_path);
    ades::validate_trace_against_graph(ades_graph, trace);
    const auto trace_sha = ades::trace_sha256(trace);
    const auto counts = ades::trace_counts(trace);
    auto boost_bundle = build_boost_graph(ades_graph);

    std::vector<ades::Distance> answers;
    answers.reserve(counts.query_count);
    std::size_t query_index = 0;
    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        ades_graph.update_weight(op.a, op.new_weight);
        boost_update(boost_bundle, op.a, op.new_weight);
      } else {
        const auto ades_answer = ades::dijkstra(ades_graph, op.a).dist.at(op.b);
        const auto boost_answer = boost_query(*boost_bundle.graph, op.a, op.b);
        if (ades_answer != boost_answer) {
          throw std::runtime_error(
              "independent oracle disagreement at query " +
              std::to_string(query_index) + ": ADES fresh Dijkstra=" +
              std::to_string(ades_answer) + " Boost.Graph Dijkstra=" +
              std::to_string(boost_answer));
        }
        answers.push_back(ades_answer);
        ++query_index;
      }
    }
    if (answers.size() != counts.query_count) {
      throw std::runtime_error("oracle answer count mismatch");
    }

    std::ofstream out(oracle_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write oracle");
    out << "ADES_ORACLE_V2 " << trace.size() << ' ' << answers.size() << ' '
        << trace_sha << '\n';
    std::size_t q = 0;
    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        out << "UPDATE " << op.a << ' ' << op.old_weight << ' '
            << op.new_weight << '\n';
      } else {
        out << "QUERY " << op.a << ' ' << op.b << ' ' << answers.at(q++) << '\n';
      }
    }
    if (!out) throw std::runtime_error("failed while writing oracle");

    std::cout << "ORACLE_FROM_TRACE_CROSS_VALIDATED"
              << " graph_sha256=" << ades::file_sha256(graph_path)
              << " trace_sha256=" << trace_sha
              << " operations=" << trace.size()
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " increases=" << counts.increase_count
              << " decreases=" << counts.decrease_count
              << " boost_version=" << BOOST_LIB_VERSION
              << " agreement=PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "oracle_from_trace: " << error.what() << '\n';
    return 2;
  }
}
