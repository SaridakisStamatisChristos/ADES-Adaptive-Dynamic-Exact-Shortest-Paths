#pragma once

#include "ades/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ades {

enum class TraceOpKind : std::uint8_t { Query, Update };

struct TraceOp {
  TraceOpKind kind{TraceOpKind::Query};
  std::uint32_t a{0};
  std::uint32_t b{0};
  Weight old_weight{0};
  Weight new_weight{0};

  static TraceOp query(std::uint32_t source, std::uint32_t target) {
    return {TraceOpKind::Query, source, target, 0, 0};
  }

  static TraceOp update(std::uint32_t edge_id, Weight old_weight,
                        Weight new_weight) {
    return {TraceOpKind::Update, edge_id, 0, old_weight, new_weight};
  }
};

struct TraceCounts {
  std::size_t query_count{0};
  std::size_t update_count{0};
  std::size_t increase_count{0};
  std::size_t decrease_count{0};
};

// Canonical operation stream used for cryptographic identity:
//   QUERY source target\n
//   UPDATE edge_id old_weight new_weight\n
std::string canonical_trace_bytes(const std::vector<TraceOp>& ops);
std::string trace_sha256(const std::vector<TraceOp>& ops);
TraceCounts trace_counts(const std::vector<TraceOp>& ops);

// Trace files contain only canonical operation lines. read_trace() accepts only
// this grammar and rejects malformed or non-canonical semantic records.
void write_trace(const std::string& path, const std::vector<TraceOp>& ops);
std::vector<TraceOp> read_trace(const std::string& path);

// Validates vertex/edge references and proves that every UPDATE old_weight
// matches the graph state produced by all preceding operations.
void validate_trace_against_graph(const Graph& initial,
                                  const std::vector<TraceOp>& ops);

}  // namespace ades
