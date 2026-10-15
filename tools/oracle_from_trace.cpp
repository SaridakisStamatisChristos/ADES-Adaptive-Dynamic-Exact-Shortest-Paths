#include "ades/baselines.hpp"
#include "ades/graph.hpp"
#include "ades/trace.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  try {
    if (argc != 4) {
      std::cerr << "usage: oracle_from_trace GRAPH TRACE ORACLE_OUT\n";
      return 2;
    }
    const std::string graph_path = argv[1];
    const std::string trace_path = argv[2];
    const std::string oracle_path = argv[3];

    auto graph = ades::Graph::load_dimacs_gr_gz(graph_path);
    const auto trace = ades::read_trace(trace_path);
    ades::validate_trace_against_graph(graph, trace);
    const auto trace_sha = ades::trace_sha256(trace);
    const auto counts = ades::trace_counts(trace);

    std::vector<ades::Distance> answers;
    answers.reserve(counts.query_count);
    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        graph.update_weight(op.a, op.new_weight);
      } else {
        answers.push_back(ades::dijkstra(graph, op.a).dist.at(op.b));
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

    std::cout << "ORACLE_FROM_TRACE"
              << " graph_sha256=" << ades::file_sha256(graph_path)
              << " trace_sha256=" << trace_sha
              << " operations=" << trace.size()
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " increases=" << counts.increase_count
              << " decreases=" << counts.decrease_count << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "oracle_from_trace: " << error.what() << '\n';
    return 2;
  }
}
