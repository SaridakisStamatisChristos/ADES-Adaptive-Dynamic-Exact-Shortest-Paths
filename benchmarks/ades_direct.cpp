#include "ades/ades.hpp"
#include "ades/memory_budget.hpp"
#include "ades/telemetry.hpp"
#include "ades/trace.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

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
  if (!end || *end != '\0') {
    throw std::runtime_error("invalid persistent-state budget");
  }
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
          op.kind != ades::TraceOpKind::Query || op.a != source ||
          op.b != target) {
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
      std::cerr << "usage: ades_direct GRAPH TRACE ORACLE REPORT.json\n";
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
    if (oracle.trace_sha256 != trace_sha ||
        oracle.answers.size() != counts.query_count) {
      throw std::runtime_error("oracle identity/count mismatch");
    }

    ades::Config base_cfg;
    base_cfg.persistent_state_budget_bytes = budget;
    if (budget != 0) {
      // PR43 equal-byte mode: the byte budget, not the historical source-count
      // cap, is the binding residency control.
      base_cfg.resident_cap = base.vertex_count();
    }
    const auto cfg = ades::config_for_scientific_ablation(
        ades::ScientificAblation::PredictiveEconomicFast, base_cfg);

    const auto preprocessing_start = Clock::now();
    ades::ADES engine(std::move(base), cfg);
    const auto preprocessing_ns = ns_since(preprocessing_start);

    std::set<std::uint32_t> unique_sources;
    std::size_t answer_index = 0;
    bool exact = true;
    std::size_t first_bad_query = std::numeric_limits<std::size_t>::max();
    ades::Distance first_got = 0, first_expected = 0;

    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        engine.update(op.a, op.new_weight);
      } else {
        unique_sources.insert(op.a);
        const auto got = engine.query(op.a, op.b);
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

    const auto stats = engine.stats();
    if (!stats.timing.reconciles()) {
      throw std::runtime_error("PR44 timing telemetry failed reconciliation");
    }
    const auto query_ns = stats.timing.query_time_ns;
    const auto update_ns = stats.timing.update_time_ns;
    const auto algorithm_ns = stats.timing.total_time_ns();
    const auto accounted = engine.accounted_algorithm_state_bytes();
    const auto peak_accounted = engine.peak_accounted_algorithm_state_bytes();
    if (accounted != stats.accounted_algorithm_state_bytes) {
      throw std::runtime_error("current persistent-state accounting mismatch");
    }
    if (budget != 0 && peak_accounted > budget) {
      throw std::runtime_error("persistent-state byte budget exceeded");
    }

    const char* commit_raw = std::getenv("ADES_CANDIDATE_COMMIT");
    const std::string candidate_commit = commit_raw ? commit_raw : "unknown";

    std::ofstream out(report_path, std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write report");
    out << "{\n";
    out << "  \"schema\": \"ades-direct-adapter-v1\",\n";
    out << "  \"candidate_commit\": "; write_json_string(out, candidate_commit); out << ",\n";
    out << "  \"profile\": \"ADES-V3\",\n";
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
    out << "  \"algorithm_ns\": " << algorithm_ns << ",\n";
    out << "  \"timing_boundary\": \"PR44 telemetry: ADES query/update operation domains only; oracle comparison, graph load, trace/oracle validation, and engine construction excluded; construction reported as preprocessing_ns\",\n";
    out << "  \"telemetry_schema_version\": " << ades::kPublicationTelemetrySchemaVersion << ",\n";
    out << "  \"persistent_state_budget_bytes\": " << budget << ",\n";
    out << "  \"accounted_algorithm_state_bytes\": " << accounted << ",\n";
    out << "  \"peak_accounted_algorithm_state_bytes\": " << peak_accounted << ",\n";
    out << "  \"accounting_version\": " << ades::kPersistentStateAccountingVersion << ",\n";
    out << "  \"accounting_boundary\": \"PR43/PR54 v2 logical persistent algorithm state; common graph input, oracle/harness state, reusable non-semantic scratch, temporaries, allocator overhead, telemetry and configuration excluded; RSS reported separately\",\n";
    out << "  \"cold_queries\": " << stats.cold_queries << ",\n";
    out << "  \"resident_queries\": " << stats.resident_queries << ",\n";
    out << "  \"promotions\": " << stats.promotions << ",\n";
    out << "  \"evictions\": " << stats.evictions << ",\n";
    out << "  \"rebuilds\": " << stats.rebuilds << ",\n";
    out << "  \"filtered_updates\": " << stats.filtered_updates << ",\n";
    out << "  \"decrease_repairs\": " << stats.decrease_repairs << ",\n";
    out << "  \"increase_repairs\": " << stats.increase_repairs << ",\n";
    out << "  \"repair_aborts\": " << stats.repair_aborts << ",\n";
    out << "  \"memory_budget_rejections\": " << stats.memory_budget_rejections << ",\n";
    out << "  \"memory_metadata_prunes\": " << stats.memory_metadata_prunes;
    if (!exact) {
      out << ",\n  \"first_mismatch\": {\"query_index\": " << first_bad_query
          << ", \"got\": " << first_got << ", \"expected\": "
          << first_expected << "}";
    }
    out << "\n}\n";
    if (!out) throw std::runtime_error("failed while writing report");

    std::cout << "ADES_DIRECT trace=" << trace_sha
              << " ops=" << trace.size()
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " increases=" << counts.increase_count
              << " decreases=" << counts.decrease_count
              << " sources=" << unique_sources.size()
              << " exact=" << (exact ? 1 : 0)
              << " algorithm_ns=" << algorithm_ns
              << " query_ns=" << query_ns
              << " update_ns=" << update_ns
              << " accounted_bytes=" << accounted
              << " peak_accounted_bytes=" << peak_accounted << '\n';
    return exact ? 0 : 3;
  } catch (const std::exception& error) {
    std::cerr << "ades_direct: " << error.what() << '\n';
    return 2;
  }
}
