#pragma once

#include "ades/graph.hpp"
#include "ades/trace.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ades {

inline constexpr std::uint32_t kWorkloadGeneratorVersion = 2;

enum class SourceFamily : std::uint8_t {
  Uniform,
  SingleHot,
  HotPool,
  Zipf,
  RotatingHot,
  Churn,
};

enum class UpdateMode : std::uint8_t {
  IncreaseOnly,
  DecreaseOnly,
  BalancedRandom,
  StrictAlternating,
  Bursty,
  RepeatedEdge,
};

enum class PerturbationMagnitude : std::uint8_t {
  Small,
  Medium,
  Large,
};

struct WorkloadConfig {
  std::size_t query_count = 1000;
  // Frozen PR46 values: 0 (static), 2, 5, 10, 50, 100 queries/update.
  std::size_t update_interval = 10;
  SourceFamily source_family = SourceFamily::HotPool;
  std::size_t hot_sources = 4;
  std::size_t epoch_queries = 250;
  // Probability [0,100] of taking the family-localized branch instead of a
  // uniform-source fallback. Uniform ignores this field but still records it.
  std::uint32_t locality_percent = 90;
  UpdateMode update_mode = UpdateMode::StrictAlternating;
  PerturbationMagnitude magnitude = PerturbationMagnitude::Medium;
  // Used only by Bursty. Total update count remains the same as the declared
  // update interval; updates are temporally clustered into these groups.
  std::size_t burst_length = 4;
};

struct WorkloadArtifact {
  std::vector<TraceOp> operations;
  TraceCounts counts{};
  std::string config_bytes;
  std::string config_sha256;
  std::string trace_sha256;
};

const char* to_string(SourceFamily value) noexcept;
const char* to_string(UpdateMode value) noexcept;
const char* to_string(PerturbationMagnitude value) noexcept;

SourceFamily parse_source_family(std::string_view value);
UpdateMode parse_update_mode(std::string_view value);
PerturbationMagnitude parse_perturbation_magnitude(std::string_view value);

void validate_workload_config(const Graph& graph, const WorkloadConfig& config);
std::string canonical_workload_config(const WorkloadConfig& config);
WorkloadArtifact generate_workload_v2(const Graph& initial,
                                      std::uint64_t seed,
                                      const WorkloadConfig& config);

}  // namespace ades
