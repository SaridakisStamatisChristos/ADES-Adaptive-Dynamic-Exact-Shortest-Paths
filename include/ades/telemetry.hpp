#pragma once
#include <cstdint>

namespace ades {

// Publication telemetry schema introduced by PR44. Any semantic change to a
// timing field requires a schema-version bump rather than silent reuse.
inline constexpr std::uint32_t kPublicationTelemetrySchemaVersion = 2;

struct TimingStats {
  // Top-level ADES operation domains. These are disjoint by construction.
  std::uint64_t query_time_ns = 0;
  std::uint64_t update_time_ns = 0;

  // Query-domain partition. These fields are disjoint and reconcile exactly
  // to query_time_ns.
  std::uint64_t cold_query_ns = 0;
  std::uint64_t resident_query_ns = 0;
  std::uint64_t query_policy_ns = 0;
  std::uint64_t promotion_ns = 0;
  std::uint64_t eviction_ns = 0;

  // Update-domain partition. These fields are disjoint and reconcile exactly
  // to update_time_ns.
  std::uint64_t graph_update_ns = 0;
  std::uint64_t decrease_repair_ns = 0;
  std::uint64_t increase_repair_ns = 0;
  std::uint64_t rebuild_ns = 0;
  std::uint64_t controller_ns = 0;
  std::uint64_t update_policy_ns = 0;

  std::uint64_t query_components_ns() const noexcept {
    return cold_query_ns + resident_query_ns + query_policy_ns + promotion_ns +
           eviction_ns;
  }

  std::uint64_t update_components_ns() const noexcept {
    return graph_update_ns + decrease_repair_ns + increase_repair_ns +
           rebuild_ns + controller_ns + update_policy_ns;
  }

  std::uint64_t total_time_ns() const noexcept {
    return query_time_ns + update_time_ns;
  }

  bool reconciles() const noexcept {
    return query_time_ns == query_components_ns() &&
           update_time_ns == update_components_ns();
  }
};

}  // namespace ades
