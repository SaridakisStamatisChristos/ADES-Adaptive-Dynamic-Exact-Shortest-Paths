#include "ades/graph.hpp"
#include "ades/trace.hpp"
#include "ades/workload.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ades;

static std::uint64_t parse_u64(const char* raw, const char* field) {
  std::string value(raw);
  std::size_t pos = 0;
  std::uint64_t parsed = 0;
  try {
    parsed = std::stoull(value, &pos);
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("invalid ") + field);
  }
  if (pos != value.size()) throw std::invalid_argument(std::string("invalid ") + field);
  return parsed;
}

static void write_metadata(const std::string& path,
                           const std::string& graph_sha256,
                           std::uint64_t seed,
                           const WorkloadConfig& config,
                           const WorkloadArtifact& artifact,
                           std::size_t vertex_count,
                           std::size_t edge_count) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write metadata: " + path);
  out << "{\n"
      << "  \"schema\": \"ades-workload-v2\",\n"
      << "  \"generator_version\": " << kWorkloadGeneratorVersion << ",\n"
      << "  \"claim_ids\": [\"C2\", \"C8\"],\n"
      << "  \"graph_sha256\": \"" << graph_sha256 << "\",\n"
      << "  \"graph_vertices\": " << vertex_count << ",\n"
      << "  \"graph_edges\": " << edge_count << ",\n"
      << "  \"seed\": " << seed << ",\n"
      << "  \"config_sha256\": \"" << artifact.config_sha256 << "\",\n"
      << "  \"trace_sha256\": \"" << artifact.trace_sha256 << "\",\n"
      << "  \"query_count\": " << artifact.counts.query_count << ",\n"
      << "  \"update_count\": " << artifact.counts.update_count << ",\n"
      << "  \"increase_count\": " << artifact.counts.increase_count << ",\n"
      << "  \"decrease_count\": " << artifact.counts.decrease_count << ",\n"
      << "  \"config\": {\n"
      << "    \"query_count\": " << config.query_count << ",\n"
      << "    \"update_interval\": " << config.update_interval << ",\n"
      << "    \"source_family\": \"" << to_string(config.source_family) << "\",\n"
      << "    \"hot_sources\": " << config.hot_sources << ",\n"
      << "    \"epoch_queries\": " << config.epoch_queries << ",\n"
      << "    \"locality_percent\": " << config.locality_percent << ",\n"
      << "    \"update_mode\": \"" << to_string(config.update_mode) << "\",\n"
      << "    \"magnitude\": \"" << to_string(config.magnitude) << "\",\n"
      << "    \"burst_length\": " << config.burst_length << "\n"
      << "  }\n"
      << "}\n";
  if (!out) throw std::runtime_error("failed while writing metadata: " + path);
}

int main(int argc, char** argv) {
  try {
    if (argc < 13 || argc > 14) {
      std::cerr
          << "usage: ades_workload_v2 GRAPH TRACE_OUT METADATA_OUT SEED QUERIES "
             "SOURCE_FAMILY UPDATE_INTERVAL UPDATE_MODE MAGNITUDE HOT_SOURCES "
             "EPOCH_QUERIES LOCALITY_PERCENT [BURST_LENGTH]\n"
          << "source families: uniform single-hot hot-pool zipf rotating-hot churn\n"
          << "update intervals: 0 2 5 10 50 100 (0 = static)\n"
          << "update modes: increase-only decrease-only balanced-random "
             "strict-alternating bursty repeated-edge\n"
          << "magnitudes: small medium large\n";
      return 2;
    }

    const std::string graph_path = argv[1];
    const std::string trace_path = argv[2];
    const std::string metadata_path = argv[3];
    const auto seed = parse_u64(argv[4], "seed");

    WorkloadConfig config;
    config.query_count = parse_u64(argv[5], "queries");
    config.source_family = parse_source_family(argv[6]);
    config.update_interval = parse_u64(argv[7], "update_interval");
    config.update_mode = parse_update_mode(argv[8]);
    config.magnitude = parse_perturbation_magnitude(argv[9]);
    config.hot_sources = parse_u64(argv[10], "hot_sources");
    config.epoch_queries = parse_u64(argv[11], "epoch_queries");
    config.locality_percent = static_cast<std::uint32_t>(parse_u64(argv[12], "locality_percent"));
    if (argc == 14) config.burst_length = parse_u64(argv[13], "burst_length");

    const auto graph_sha = file_sha256(graph_path);
    auto graph = Graph::load_dimacs_gr_gz(graph_path);
    const auto artifact = generate_workload_v2(graph, seed, config);

    write_trace(trace_path, artifact.operations);
    const auto persisted = read_trace(trace_path);
    validate_trace_against_graph(graph, persisted);
    const auto persisted_sha = file_sha256(trace_path);
    if (persisted_sha != artifact.trace_sha256 ||
        trace_sha256(persisted) != artifact.trace_sha256) {
      throw std::runtime_error("persisted canonical trace does not match generated trace identity");
    }

    write_metadata(metadata_path, graph_sha, seed, config, artifact,
                   graph.vertex_count(), graph.edge_count());

    std::cout << "WORKLOAD_V2"
              << " graph_sha256=" << graph_sha
              << " config_sha256=" << artifact.config_sha256
              << " trace_sha256=" << artifact.trace_sha256
              << " queries=" << artifact.counts.query_count
              << " updates=" << artifact.counts.update_count
              << " increases=" << artifact.counts.increase_count
              << " decreases=" << artifact.counts.decrease_count
              << " seed=" << seed << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ades_workload_v2: " << error.what() << '\n';
    return 2;
  }
}
