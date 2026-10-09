#pragma once

#include "ades/memory_budget.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ades {

// Persistent forward half of an exact bidirectional Dijkstra query. The
// distance labels, selected parent edges and indexed heap survive between
// targets for one source while the graph epoch is unchanged.
struct ProgressiveFrontier {
  std::uint32_t source = 0;
  std::vector<Distance> dist;
  std::vector<std::int64_t> parent_edge;
  // -1 = unseen, -2 = settled, >=0 = index in heap_vertices.
  std::vector<std::int64_t> heap_pos;
  std::vector<std::uint32_t> heap_vertices;
  std::uint64_t forward_edge_scans = 0;
  std::uint64_t settled_vertices = 0;
  bool complete = false;

  ProgressiveFrontier() = default;
  ProgressiveFrontier(std::size_t vertex_count, std::uint32_t source_vertex);
};

struct ProgressiveQueryResult {
  Distance distance = INF;
  std::uint64_t forward_edge_scans = 0;
  std::uint64_t backward_edge_scans = 0;
  std::uint64_t forward_settled = 0;
  bool became_complete = false;
};

ProgressiveQueryResult progressive_bidirectional_query(
    const Graph& graph, ProgressiveFrontier& frontier, std::uint32_t target);

// Completes the remaining forward Dijkstra work and converts the frontier into
// the ordinary repairable SSSP representation. Existing distance/parent arrays
// are moved rather than recomputed.
SSSPState complete_progressive_sssp(const Graph& graph,
                                    ProgressiveFrontier& frontier);

// PR52 accounts the heap at its full vertex bound, not its instantaneous size.
// This deliberately reserves the worst-case semantic frontier capacity up
// front, so progressive state cannot grow beyond the declared byte budget.
inline std::uint64_t progressive_frontier_accounted_bytes_for_vertices(
    std::size_t n) {
  std::uint64_t bytes = sizeof(std::uint32_t) + 2u * sizeof(std::uint64_t) +
                        sizeof(std::uint8_t);
  bytes = accounted_add(bytes,
                        accounted_mul(static_cast<std::uint64_t>(n),
                                      sizeof(Distance)));
  bytes = accounted_add(bytes,
                        accounted_mul(static_cast<std::uint64_t>(n),
                                      sizeof(std::int64_t)));
  bytes = accounted_add(bytes,
                        accounted_mul(static_cast<std::uint64_t>(n),
                                      sizeof(std::int64_t)));
  bytes = accounted_add(bytes,
                        accounted_mul(static_cast<std::uint64_t>(n),
                                      sizeof(std::uint32_t)));
  return bytes;
}

}  // namespace ades
