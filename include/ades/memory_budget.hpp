#pragma once

#include "ades/repair_controller.hpp"
#include "ades/shortest_paths.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ades {

// Publication persistent-state accounting model introduced by PR43.
//
// The budget is intentionally a logical algorithm-state budget, not RSS.
// It includes semantic persistent state that differs between algorithms and
// scales with residency/admission decisions. It excludes the common graph,
// allocator/container implementation overhead, temporary workspaces and
// telemetry/configuration. Peak RSS is recorded independently by benchmark
// harnesses.
inline constexpr std::uint32_t kPersistentStateAccountingVersion = 1;

inline std::uint64_t accounted_add(std::uint64_t a, std::uint64_t b) {
  if (b > std::numeric_limits<std::uint64_t>::max() - a)
    throw std::overflow_error("persistent-state byte accounting overflow");
  return a + b;
}

inline std::uint64_t accounted_mul(std::uint64_t a, std::uint64_t b) {
  if (a && b > std::numeric_limits<std::uint64_t>::max() / a)
    throw std::overflow_error("persistent-state byte accounting overflow");
  return a * b;
}

template <class T>
inline std::uint64_t logical_vector_bytes(const std::vector<T>& values) {
  return accounted_mul(static_cast<std::uint64_t>(values.size()), sizeof(T));
}

// SSSPState has two persistent scalar fields plus six logical vectors.
// Vector control blocks and allocator slack are deliberately excluded from the
// logical model; their physical effect remains visible in peak RSS.
inline std::uint64_t sssp_state_accounted_bytes(const SSSPState& state) {
  std::uint64_t bytes = 2u * sizeof(std::uint32_t); // source + repair_epoch
  bytes = accounted_add(bytes, logical_vector_bytes(state.dist));
  bytes = accounted_add(bytes, logical_vector_bytes(state.parent_edge));
  bytes = accounted_add(bytes, logical_vector_bytes(state.first_child));
  bytes = accounted_add(bytes, logical_vector_bytes(state.next_sibling));
  bytes = accounted_add(bytes, logical_vector_bytes(state.prev_sibling));
  bytes = accounted_add(bytes, logical_vector_bytes(state.repair_mark));
  return bytes;
}

inline std::uint64_t sssp_state_accounted_bytes_for_vertices(std::size_t n) {
  constexpr std::uint64_t per_vertex =
      sizeof(Distance) + 4u * sizeof(std::int64_t) + sizeof(std::uint32_t);
  return accounted_add(2u * sizeof(std::uint32_t),
                       accounted_mul(static_cast<std::uint64_t>(n), per_vertex));
}

// B2L/B3L logical metadata: one map key and one LRU-list source id.
inline constexpr std::uint64_t bounded_resident_metadata_bytes() {
  return 2u * sizeof(std::uint32_t);
}

inline std::uint64_t bounded_resident_entry_accounted_bytes(
    const SSSPState& state) {
  return accounted_add(sssp_state_accounted_bytes(state),
                       bounded_resident_metadata_bytes());
}

inline std::uint64_t bounded_resident_entry_accounted_bytes_for_vertices(
    std::size_t n) {
  return accounted_add(sssp_state_accounted_bytes_for_vertices(n),
                       bounded_resident_metadata_bytes());
}

// ADES resident metadata: unordered-map source key; hit, recency and update
// debt counters; and the persistent repair-controller model state.
inline constexpr std::uint64_t repair_controller_accounted_bytes() {
  // RepairController: seven doubles plus two size_t hard ceilings.
  return 7u * sizeof(double) + 2u * sizeof(std::size_t);
}

inline constexpr std::uint64_t ades_resident_metadata_bytes() {
  return sizeof(std::uint32_t) + 3u * sizeof(std::uint64_t) +
         repair_controller_accounted_bytes();
}

inline std::uint64_t ades_resident_entry_accounted_bytes(
    const SSSPState& state) {
  return accounted_add(sssp_state_accounted_bytes(state),
                       ades_resident_metadata_bytes());
}

inline std::uint64_t ades_resident_entry_accounted_bytes_for_vertices(
    std::size_t n) {
  return accounted_add(sssp_state_accounted_bytes_for_vertices(n),
                       ades_resident_metadata_bytes());
}

// ADES-v1 nonresident persistent metadata.
inline constexpr std::uint64_t ades_probation_entry_accounted_bytes() {
  return sizeof(std::uint32_t) + sizeof(std::uint32_t) + sizeof(std::uint64_t);
}

inline constexpr std::uint64_t ades_cooldown_entry_accounted_bytes() {
  return sizeof(std::uint32_t) + sizeof(std::uint64_t);
}

// PR50 ADES-v2 predictive-economic metadata. The accounting is logical: each
// map entry includes its source key and semantic fields, while allocator/node
// overhead remains represented by RSS just like the older policies.
inline constexpr std::uint64_t ades_economic_source_accounted_bytes() {
  return sizeof(std::uint32_t) + 2u * sizeof(std::uint64_t) + 3u * sizeof(double);
}

inline constexpr std::uint64_t ades_economic_resident_accounted_bytes() {
  return sizeof(std::uint32_t) + sizeof(std::uint64_t) + 3u * sizeof(double);
}

inline constexpr std::uint64_t ades_economic_recent_slot_accounted_bytes() {
  return sizeof(std::uint32_t) + 3u * sizeof(std::uint64_t) + sizeof(std::uint8_t);
}

inline std::uint64_t ades_economic_recent_table_accounted_bytes(std::size_t slots) {
  return accounted_mul(static_cast<std::uint64_t>(slots),
                       ades_economic_recent_slot_accounted_bytes());
}

} // namespace ades