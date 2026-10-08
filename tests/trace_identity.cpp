#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/trace.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ades;

namespace {

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "trace identity test failure: " << message << '\n';
  std::exit(1);
}

void require(bool condition, const std::string& message) {
  if (!condition) fail(message);
}

std::vector<Distance> oracle_answers(Graph graph,
                                     const std::vector<TraceOp>& ops) {
  std::vector<Distance> answers;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) {
      graph.update_weight(op.a, op.new_weight);
    } else {
      answers.push_back(dijkstra(graph, op.a).dist.at(op.b));
    }
  }
  return answers;
}

template <typename Engine>
std::vector<Distance> engine_answers(Engine& engine,
                                     const std::vector<TraceOp>& ops) {
  std::vector<Distance> answers;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) {
      engine.update(op.a, op.new_weight);
    } else {
      answers.push_back(engine.query(op.a, op.b));
    }
  }
  return answers;
}

struct FreshDijkstra {
  Graph graph;
  Distance query(std::uint32_t source, std::uint32_t target) {
    return dijkstra(graph, source).dist.at(target);
  }
  void update(std::uint32_t edge_id, Weight weight) {
    graph.update_weight(edge_id, weight);
  }
};

struct FreshBidirectional {
  Graph graph;
  Distance query(std::uint32_t source, std::uint32_t target) {
    return bidirectional_dijkstra(graph, source, target);
  }
  void update(std::uint32_t edge_id, Weight weight) {
    graph.update_weight(edge_id, weight);
  }
};

Graph fixture_graph() {
  Graph graph(5);
  graph.add_edge(0, 1, 4);   // id 0
  graph.add_edge(1, 2, 3);   // id 1
  graph.add_edge(0, 2, 10);  // id 2
  graph.add_edge(2, 3, 2);   // id 3
  graph.add_edge(1, 3, 10);  // id 4
  graph.add_edge(3, 4, 1);   // id 5
  graph.add_edge(0, 4, 30);  // id 6
  return graph;
}

}  // namespace

int main() {
  const std::vector<TraceOp> trace = {
      TraceOp::query(0, 4),
      TraceOp::update(2, 10, 5),
      TraceOp::query(0, 3),
      TraceOp::update(1, 3, 8),
      TraceOp::query(0, 4),
  };

  const auto graph = fixture_graph();
  validate_trace_against_graph(graph, trace);

  require(trace_sha256({}) ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256 implementation fails the empty-message test vector");
  require(trace_sha256(trace) ==
              "dac082c34b7b99ced8f8e9591903694024c4107ac0da61949982187f894a331b",
          "canonical trace SHA-256 changed unexpectedly");

  const auto counts = trace_counts(trace);
  require(counts.query_count == 3, "wrong query count");
  require(counts.update_count == 2, "wrong update count");
  require(counts.increase_count == 1, "wrong increase count");
  require(counts.decrease_count == 1, "wrong decrease count");

  const auto path =
      std::filesystem::temp_directory_path() / "ades-pr42-trace-identity.trace";
  write_trace(path.string(), trace);
  const auto replay = read_trace(path.string());
  std::filesystem::remove(path);

  require(canonical_trace_bytes(replay) == canonical_trace_bytes(trace),
          "replay changed canonical operation bytes");
  require(trace_sha256(replay) == trace_sha256(trace),
          "replay changed trace SHA-256");
  require(trace_counts(replay).query_count == counts.query_count &&
              trace_counts(replay).update_count == counts.update_count &&
              trace_counts(replay).increase_count == counts.increase_count &&
              trace_counts(replay).decrease_count == counts.decrease_count,
          "replay changed operation counts");
  validate_trace_against_graph(graph, replay);

  const auto reference = oracle_answers(graph, replay);

  FreshDijkstra b0{graph};
  require(engine_answers(b0, replay) == reference,
          "B0 answers differ from oracle");

  FreshBidirectional b1{graph};
  require(engine_answers(b1, replay) == reference,
          "B1 answers differ from oracle");

  AlwaysResident b2(graph, ResidentMode::FullRebuild);
  require(engine_answers(b2, replay) == reference,
          "B2 answers differ from oracle");

  AlwaysResident b3(graph, ResidentMode::LocalRepair);
  require(engine_answers(b3, replay) == reference,
          "B3 answers differ from oracle");

  Config cfg;
  cfg.resident_cap = 2;
  cfg.probation_queries = 1;
  cfg.promotion_ratio = 0.0;
  ADES b4(graph, cfg);
  require(engine_answers(b4, replay) == reference,
          "B4 answers differ from oracle");

  auto tampered = replay;
  tampered[1].old_weight = 11;
  bool rejected = false;
  try {
    validate_trace_against_graph(graph, tampered);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "old_weight tampering was not rejected");

  std::cout << "trace identity exact: sha256=" << trace_sha256(replay)
            << " queries=" << counts.query_count
            << " updates=" << counts.update_count
            << " increases=" << counts.increase_count
            << " decreases=" << counts.decrease_count << '\n';
  return 0;
}
