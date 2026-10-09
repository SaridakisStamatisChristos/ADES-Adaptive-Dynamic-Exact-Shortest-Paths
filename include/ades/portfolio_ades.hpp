#pragma once

#include "ades/hazard_ades.hpp"
#include "ades/memory_budget.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ades {

struct PortfolioStats {
  std::uint64_t cold_queries = 0;
  std::uint64_t resident_queries = 0;
  std::uint64_t promotions = 0;
  std::uint64_t evictions = 0;
  std::uint64_t rebuilds = 0;
  std::uint64_t accounted_algorithm_state_bytes = 0;
  std::uint64_t peak_accounted_algorithm_state_bytes = 0;
};

// Development-only V5 portfolio refinement. For low-branching, low-skew
// graphs where the full configured working set fits in the byte budget, warm
// at most resident_cap probationary SSSPs on first touch. Once those slots are
// full, unseen sources remain cold; a repeated source may replace only an
// unproven (never-hit) probation entry. This prevents B3L-style endless miss
// builds while removing the first-touch penalty in the regime where full-state
// construction is structurally cheap. Other graphs delegate unchanged to the
// hazard-aware V5 controller.
class PortfolioADES {
  struct Entry {
    SSSPState state;
    std::uint64_t hits = 0;
    std::uint64_t last_query = 0;
  };
  struct Recent {
    std::uint32_t source = 0;
    std::uint64_t last_query = 0;
    bool valid = false;
  };

  Graph graph_;
  HazardConfig cfg_;
  bool eager_mode_ = false;
  std::unique_ptr<HazardAwareADES> fallback_;
  std::unordered_map<std::uint32_t, Entry> states_;
  static constexpr std::size_t kRecentSlots = 64;
  std::array<Recent, kRecentSlots> recent_{};
  std::uint64_t query_clock_ = 0;
  PortfolioStats stats_{};

  std::uint64_t recent_bytes() const;
  std::uint64_t entry_bytes() const;
  std::uint64_t accounted_bytes() const;
  void refresh_memory();
  bool can_fit_entry() const;
  bool graph_favors_eager() const;
  std::unordered_map<std::uint32_t, Entry>::iterator probation_victim();
  void remember(std::uint32_t source);

 public:
  explicit PortfolioADES(Graph graph, HazardConfig config = {});
  Distance query(std::uint32_t source, std::uint32_t target);
  void update(std::uint32_t edge_id, Weight new_weight);

  bool eager_mode() const noexcept { return eager_mode_; }
  const PortfolioStats& stats() const noexcept { return stats_; }
  std::uint64_t accounted_algorithm_state_bytes() const;
  std::uint64_t peak_accounted_algorithm_state_bytes() const;
};

}  // namespace ades
