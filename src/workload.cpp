#include "ades/workload.hpp"

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ades {
namespace {

std::uint64_t bounded(std::mt19937_64& rng, std::uint64_t bound) {
  if (!bound) throw std::logic_error("bounded random draw with zero bound");
  return rng() % bound;
}

bool localized(std::mt19937_64& rng, std::uint32_t percent) {
  if (percent == 100) return true;
  if (percent == 0) return false;
  return bounded(rng, 100) < percent;
}

std::vector<std::uint32_t> deterministic_hot_pool(std::size_t n,
                                                  std::size_t k,
                                                  std::mt19937_64& rng) {
  std::vector<std::uint32_t> vertices(n);
  for (std::uint32_t i = 0; i < n; ++i) vertices[i] = i;
  for (std::size_t i = 0; i < k; ++i) {
    const auto j = i + static_cast<std::size_t>(bounded(rng, n - i));
    std::swap(vertices[i], vertices[j]);
  }
  vertices.resize(k);
  return vertices;
}

std::uint32_t sample_zipf_rank(const std::vector<std::uint32_t>& hot,
                               std::mt19937_64& rng) {
  constexpr std::uint64_t scale = 1'000'000;
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < hot.size(); ++i) total += scale / (i + 1);
  auto draw = bounded(rng, total);
  for (std::size_t i = 0; i < hot.size(); ++i) {
    const auto weight = scale / (i + 1);
    if (draw < weight) return hot[i];
    draw -= weight;
  }
  return hot.back();
}

std::uint32_t pick_source(SourceFamily family,
                          const std::vector<std::uint32_t>& hot,
                          std::size_t query_index,
                          const WorkloadConfig& config,
                          std::size_t vertex_count,
                          std::mt19937_64& rng) {
  const auto uniform = [&] {
    return static_cast<std::uint32_t>(bounded(rng, vertex_count));
  };
  if (family == SourceFamily::Uniform) return uniform();
  if (!localized(rng, config.locality_percent)) return uniform();

  switch (family) {
    case SourceFamily::SingleHot:
      return hot.front();
    case SourceFamily::HotPool:
      return hot[bounded(rng, hot.size())];
    case SourceFamily::Zipf:
      return sample_zipf_rank(hot, rng);
    case SourceFamily::RotatingHot:
      return hot[(query_index / config.epoch_queries) % hot.size()];
    case SourceFamily::Churn:
      return hot[query_index % hot.size()];
    case SourceFamily::Uniform:
      break;
  }
  throw std::logic_error("unreachable source family");
}

std::pair<Weight, Weight> magnitude_bounds(PerturbationMagnitude magnitude) {
  switch (magnitude) {
    case PerturbationMagnitude::Small:
      return {1, 3};
    case PerturbationMagnitude::Medium:
      return {4, 31};
    case PerturbationMagnitude::Large:
      return {32, 255};
  }
  throw std::logic_error("unknown perturbation magnitude");
}

struct EdgeChoice {
  std::uint32_t id = 0;
  Weight delta = 0;
};

EdgeChoice choose_edge(const Graph& graph,
                       bool increase,
                       PerturbationMagnitude magnitude,
                       std::mt19937_64& rng,
                       bool require_both_directions = false) {
  const auto [min_delta, max_delta] = magnitude_bounds(magnitude);
  if (!graph.edge_count()) throw std::runtime_error("workload update requires at least one edge");
  const auto start = static_cast<std::size_t>(bounded(rng, graph.edge_count()));
  constexpr Weight max_weight = std::numeric_limits<Weight>::max();

  for (std::size_t offset = 0; offset < graph.edge_count(); ++offset) {
    const auto id = static_cast<std::uint32_t>((start + offset) % graph.edge_count());
    const auto weight = graph.edge(id).weight;
    Weight capacity = 0;
    if (require_both_directions) {
      capacity = std::min(weight, max_weight - weight);
    } else if (increase) {
      capacity = max_weight - weight;
    } else {
      capacity = weight;
    }
    if (capacity < min_delta) continue;
    const auto upper = std::min(max_delta, capacity);
    const auto delta = min_delta + bounded(rng, upper - min_delta + 1);
    return {id, delta};
  }

  throw std::runtime_error(
      require_both_directions
          ? "no edge supports the requested repeated-edge perturbation magnitude"
          : (increase ? "no edge supports the requested increase magnitude"
                      : "no edge supports the requested decrease magnitude"));
}

TraceOp directional_update(Graph& graph,
                           bool increase,
                           PerturbationMagnitude magnitude,
                           std::mt19937_64& rng) {
  const auto choice = choose_edge(graph, increase, magnitude, rng);
  const auto old_weight = graph.edge(choice.id).weight;
  const auto new_weight = increase ? old_weight + choice.delta
                                   : old_weight - choice.delta;
  graph.update_weight(choice.id, new_weight);
  return TraceOp::update(choice.id, old_weight, new_weight);
}

struct RepeatedEdgeState {
  bool initialized = false;
  std::uint32_t edge_id = 0;
  Weight base_weight = 0;
  Weight delta = 0;
};

TraceOp repeated_edge_update(Graph& graph,
                             std::size_t update_index,
                             PerturbationMagnitude magnitude,
                             std::mt19937_64& rng,
                             RepeatedEdgeState& state) {
  if (!state.initialized) {
    const auto choice = choose_edge(graph, true, magnitude, rng, true);
    state.initialized = true;
    state.edge_id = choice.id;
    state.base_weight = graph.edge(choice.id).weight;
    state.delta = choice.delta;
  }

  const auto old_weight = graph.edge(state.edge_id).weight;
  const bool increase = (update_index % 2) == 0;
  const auto expected = increase ? state.base_weight : state.base_weight + state.delta;
  if (old_weight != expected) {
    throw std::logic_error("repeated-edge workload lost its deterministic anchor state");
  }
  const auto new_weight = increase ? state.base_weight + state.delta : state.base_weight;
  graph.update_weight(state.edge_id, new_weight);
  return TraceOp::update(state.edge_id, old_weight, new_weight);
}

std::vector<std::size_t> update_schedule(const WorkloadConfig& config) {
  std::vector<std::size_t> at_query(config.query_count, 0);
  if (!config.update_interval || config.query_count <= 1) return at_query;
  const auto planned = (config.query_count - 1) / config.update_interval;
  if (config.update_mode != UpdateMode::Bursty) {
    for (std::size_t i = 1; i <= planned; ++i)
      at_query[i * config.update_interval] = 1;
    return at_query;
  }

  for (std::size_t start = 0; start < planned; start += config.burst_length) {
    const auto count = std::min(config.burst_length, planned - start);
    const auto last_nominal_index = start + count;
    const auto query_index = std::min(config.query_count - 1,
                                      last_nominal_index * config.update_interval);
    at_query[query_index] += count;
  }
  return at_query;
}

TraceOp make_update(Graph& graph,
                    std::size_t update_index,
                    const WorkloadConfig& config,
                    std::mt19937_64& rng,
                    RepeatedEdgeState& repeated_state) {
  switch (config.update_mode) {
    case UpdateMode::IncreaseOnly:
      return directional_update(graph, true, config.magnitude, rng);
    case UpdateMode::DecreaseOnly:
      return directional_update(graph, false, config.magnitude, rng);
    case UpdateMode::BalancedRandom:
      return directional_update(graph, (rng() & 1U) != 0, config.magnitude, rng);
    case UpdateMode::StrictAlternating:
    case UpdateMode::Bursty:
      return directional_update(graph, (update_index % 2) == 0, config.magnitude, rng);
    case UpdateMode::RepeatedEdge:
      return repeated_edge_update(graph, update_index, config.magnitude, rng,
                                  repeated_state);
  }
  throw std::logic_error("unknown update mode");
}

bool allowed_update_interval(std::size_t interval) {
  return interval == 0 || interval == 2 || interval == 5 || interval == 10 ||
         interval == 50 || interval == 100;
}

}  // namespace

const char* to_string(SourceFamily value) noexcept {
  switch (value) {
    case SourceFamily::Uniform: return "uniform";
    case SourceFamily::SingleHot: return "single-hot";
    case SourceFamily::HotPool: return "hot-pool";
    case SourceFamily::Zipf: return "zipf";
    case SourceFamily::RotatingHot: return "rotating-hot";
    case SourceFamily::Churn: return "churn";
  }
  return "unknown";
}

const char* to_string(UpdateMode value) noexcept {
  switch (value) {
    case UpdateMode::IncreaseOnly: return "increase-only";
    case UpdateMode::DecreaseOnly: return "decrease-only";
    case UpdateMode::BalancedRandom: return "balanced-random";
    case UpdateMode::StrictAlternating: return "strict-alternating";
    case UpdateMode::Bursty: return "bursty";
    case UpdateMode::RepeatedEdge: return "repeated-edge";
  }
  return "unknown";
}

const char* to_string(PerturbationMagnitude value) noexcept {
  switch (value) {
    case PerturbationMagnitude::Small: return "small";
    case PerturbationMagnitude::Medium: return "medium";
    case PerturbationMagnitude::Large: return "large";
  }
  return "unknown";
}

SourceFamily parse_source_family(std::string_view value) {
  if (value == "uniform") return SourceFamily::Uniform;
  if (value == "single-hot") return SourceFamily::SingleHot;
  if (value == "hot-pool") return SourceFamily::HotPool;
  if (value == "zipf") return SourceFamily::Zipf;
  if (value == "rotating-hot") return SourceFamily::RotatingHot;
  if (value == "churn") return SourceFamily::Churn;
  throw std::invalid_argument("unknown source family: " + std::string(value));
}

UpdateMode parse_update_mode(std::string_view value) {
  if (value == "increase-only") return UpdateMode::IncreaseOnly;
  if (value == "decrease-only") return UpdateMode::DecreaseOnly;
  if (value == "balanced-random") return UpdateMode::BalancedRandom;
  if (value == "strict-alternating") return UpdateMode::StrictAlternating;
  if (value == "bursty") return UpdateMode::Bursty;
  if (value == "repeated-edge") return UpdateMode::RepeatedEdge;
  throw std::invalid_argument("unknown update mode: " + std::string(value));
}

PerturbationMagnitude parse_perturbation_magnitude(std::string_view value) {
  if (value == "small") return PerturbationMagnitude::Small;
  if (value == "medium") return PerturbationMagnitude::Medium;
  if (value == "large") return PerturbationMagnitude::Large;
  throw std::invalid_argument("unknown perturbation magnitude: " + std::string(value));
}

void validate_workload_config(const Graph& graph, const WorkloadConfig& config) {
  if (!graph.vertex_count()) throw std::invalid_argument("workload graph has no vertices");
  if (!config.query_count) throw std::invalid_argument("query_count must be positive");
  if (!allowed_update_interval(config.update_interval))
    throw std::invalid_argument("update_interval must be one of 0,2,5,10,50,100");
  if (!config.hot_sources || config.hot_sources > graph.vertex_count())
    throw std::invalid_argument("hot_sources must be in [1, vertex_count]");
  if (!config.epoch_queries) throw std::invalid_argument("epoch_queries must be positive");
  if (config.locality_percent > 100)
    throw std::invalid_argument("locality_percent must be in [0,100]");
  if (!config.burst_length) throw std::invalid_argument("burst_length must be positive");
  if (config.update_interval && !graph.edge_count())
    throw std::invalid_argument("dynamic workload requires at least one edge");
}

std::string canonical_workload_config(const WorkloadConfig& config) {
  std::string out;
  out += "workload_generator_version=" + std::to_string(kWorkloadGeneratorVersion) + "\n";
  out += "query_count=" + std::to_string(config.query_count) + "\n";
  out += "update_interval=" + std::to_string(config.update_interval) + "\n";
  out += "source_family=" + std::string(to_string(config.source_family)) + "\n";
  out += "hot_sources=" + std::to_string(config.hot_sources) + "\n";
  out += "epoch_queries=" + std::to_string(config.epoch_queries) + "\n";
  out += "locality_percent=" + std::to_string(config.locality_percent) + "\n";
  out += "update_mode=" + std::string(to_string(config.update_mode)) + "\n";
  out += "magnitude=" + std::string(to_string(config.magnitude)) + "\n";
  out += "burst_length=" + std::to_string(config.burst_length) + "\n";
  return out;
}

WorkloadArtifact generate_workload_v2(const Graph& initial,
                                      std::uint64_t seed,
                                      const WorkloadConfig& config) {
  validate_workload_config(initial, config);
  Graph graph = initial;
  std::mt19937_64 rng(seed);
  const auto hot = deterministic_hot_pool(graph.vertex_count(), config.hot_sources, rng);
  const auto schedule = update_schedule(config);
  const auto planned_updates = config.update_interval
                                   ? (config.query_count - 1) / config.update_interval
                                   : 0;

  WorkloadArtifact artifact;
  artifact.operations.reserve(config.query_count + planned_updates);
  artifact.config_bytes = canonical_workload_config(config);
  artifact.config_sha256 = sha256_bytes(artifact.config_bytes);

  RepeatedEdgeState repeated_state;
  std::size_t update_index = 0;
  for (std::size_t query_index = 0; query_index < config.query_count; ++query_index) {
    for (std::size_t j = 0; j < schedule[query_index]; ++j) {
      artifact.operations.push_back(
          make_update(graph, update_index++, config, rng, repeated_state));
    }
    const auto source = pick_source(config.source_family, hot, query_index, config,
                                    graph.vertex_count(), rng);
    const auto target = static_cast<std::uint32_t>(bounded(rng, graph.vertex_count()));
    artifact.operations.push_back(TraceOp::query(source, target));
  }

  validate_trace_against_graph(initial, artifact.operations);
  artifact.counts = trace_counts(artifact.operations);
  artifact.trace_sha256 = trace_sha256(artifact.operations);
  if (artifact.counts.query_count != config.query_count ||
      artifact.counts.update_count != planned_updates) {
    throw std::logic_error("workload generator operation-count invariant failed");
  }
  return artifact;
}

}  // namespace ades
