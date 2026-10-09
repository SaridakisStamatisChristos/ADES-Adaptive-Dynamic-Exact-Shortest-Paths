#pragma once

#include "ades/progressive_frontier.hpp"
#include "ades/repair_controller.hpp"
#include "ades/telemetry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace ades {

// ADES-V5: a three-lane causal controller.
//
// 1. Direct full residency when measured reuse already repays a complete SSSP
//    and the state fits without displacing a proven resident.
// 2. Progressive frontier only when the source has repeated in the current
//    graph epoch and the observed update process predicts enough time for
//    additional reuse before invalidation.
// 3. Exact cold bidirectional search otherwise.
//
// No workload labels or future trace information are consumed.
struct HazardConfig {
  std::size_t resident_cap = 4;
  std::size_t partial_cap = 8;
  std::uint64_t persistent_state_budget_bytes = 0;
  std::uint64_t horizon_queries = 32;
  std::uint32_t full_min_observations = 3;
  double ewma_alpha = 0.25;

  // Progressive-state survival gate.
  double survival_margin = 1.50;
  std::uint64_t startup_max_reuse_gap = 4;
  std::uint32_t min_update_gap_observations = 2;

  // Direct full-residency gate. The candidate must be economically positive
  // and fit without resident eviction. This intentionally prevents the PR51
  // premature replacement/thrashing failure mode.
  double cold_maintenance_proxy_fraction = 0.10;
  double direct_value_margin = 1.05;

  RepairBudget repair_safety_ceiling{1u << 20, 1u << 22};
};

struct HazardStats {
  std::uint64_t cold_queries = 0;
  std::uint64_t resident_queries = 0;
  std::uint64_t progressive_queries = 0;
  std::uint64_t promotions = 0;
  std::uint64_t direct_promotions = 0;
  std::uint64_t progressive_promotions = 0;
  std::uint64_t evictions = 0;
  std::uint64_t rebuilds = 0;
  std::uint64_t repair_aborts = 0;
  std::uint64_t filtered_updates = 0;
  std::uint64_t decrease_repairs = 0;
  std::uint64_t increase_repairs = 0;
  std::uint64_t economic_candidates = 0;
  std::uint64_t economic_rejections = 0;
  std::uint64_t memory_budget_rejections = 0;
  std::uint64_t partial_creations = 0;
  std::uint64_t partial_evictions = 0;
  std::uint64_t partial_invalidations = 0;
  std::uint64_t partial_completions = 0;
  std::uint64_t hazard_rejections = 0;
  std::uint64_t direct_fit_rejections = 0;
  std::uint64_t accounted_algorithm_state_bytes = 0;
  std::uint64_t peak_accounted_algorithm_state_bytes = 0;
  std::uint64_t persistent_state_budget_bytes = 0;
  TimingStats timing{};
};

class HazardAwareADES {
  Graph graph_;
  HazardConfig cfg_;
  HazardStats stats_;
  std::uint64_t query_clock_ = 0;
  std::uint64_t update_clock_ = 0;
  std::uint64_t graph_epoch_ = 0;
  std::uint64_t last_update_query_clock_ = 0;
  std::uint64_t update_gap_observations_ = 0;
  double update_query_gap_ewma_ = 0.0;
  double maintenance_ns_ewma_ = 0.0;
  double full_build_ns_ewma_ = 0.0;

  static constexpr std::size_t kRecentSlots = 64;
  struct Recent {
    std::uint32_t source = 0;
    std::uint64_t last_query = 0;
    std::uint64_t update_clock = 0;
    std::uint64_t cold_ns = 0;
    std::uint64_t edge_scans = 0;
    bool valid = false;
  };

  struct PartialEntry {
    ProgressiveFrontier frontier;
    std::uint64_t graph_epoch = 0;
    std::uint64_t last_query = 0;
    std::uint64_t observations = 0;
    double reuse_gap_ewma = 0.0;
    double cold_ns_ewma = 0.0;
    double cold_edge_scans_ewma = 0.0;
    double partial_query_ns_ewma = 0.0;
  };

  struct ResidentEntry {
    SSSPState state;
    RepairController controller{};
    std::uint64_t hits = 0;
    std::uint64_t last_query = 0;
    std::uint64_t observations = 0;
    double reuse_gap_ewma = 0.0;
    double cold_ns_ewma = 0.0;
    double maintenance_ns_ewma = 0.0;
  };

  std::array<Recent, kRecentSlots> recent_{};
  std::unordered_map<std::uint32_t, PartialEntry> partials_;
  std::unordered_map<std::uint32_t, ResidentEntry> residents_;

  std::uint64_t recent_table_bytes() const;
  std::uint64_t partial_entry_bytes() const;
  std::uint64_t resident_entry_bytes() const;
  std::uint64_t accounted_bytes_impl() const;
  void refresh_accounted_memory();
  bool can_fit(std::uint64_t extra) const;
  bool can_fit_resident_without_eviction(std::uint32_t source) const;

  void remember(std::uint32_t source, std::uint64_t last_query,
                double cold_ns, double edge_scans);
  void remember(const PartialEntry& entry);
  void erase_partial(std::unordered_map<std::uint32_t, PartialEntry>::iterator it,
                     bool invalidation, bool eviction);
  void prune_stale_partials();
  bool ensure_partial_room(
      std::uint64_t required,
      std::uint32_t protected_source = UINT32_MAX);

  double estimate_full_build_ns(double cold_ns, double edge_scans) const;
  double estimate_full_build_ns(const PartialEntry& entry) const;
  double direct_candidate_value(const Recent& recent,
                                std::uint64_t reuse_gap) const;
  double partial_promotion_value(const PartialEntry& entry) const;
  double expected_queries_to_next_update() const;
  bool progressive_survival_gate(const Recent& recent,
                                 std::uint64_t reuse_gap) const;

  bool admit_direct(std::uint32_t source, SSSPState state,
                    std::uint64_t build_ns, const Recent& recent,
                    std::uint64_t reuse_gap);
  bool admit_completed(std::uint32_t source, SSSPState state,
                       std::uint64_t build_ns, const PartialEntry& history);
  void record_maintenance(ResidentEntry& entry, std::uint64_t ns);

 public:
  explicit HazardAwareADES(Graph graph, HazardConfig config = {});

  Distance query(std::uint32_t source, std::uint32_t target);
  void update(std::uint32_t edge_id, Weight new_weight);

  const Graph& graph() const noexcept { return graph_; }
  const HazardConfig& config() const noexcept { return cfg_; }
  const HazardStats& stats() const noexcept { return stats_; }
  bool resident(std::uint32_t source) const { return residents_.contains(source); }
  bool partial(std::uint32_t source) const { return partials_.contains(source); }
  std::size_t resident_count() const noexcept { return residents_.size(); }
  std::size_t partial_count() const noexcept { return partials_.size(); }
  std::uint64_t accounted_algorithm_state_bytes() const {
    return accounted_bytes_impl();
  }
  std::uint64_t peak_accounted_algorithm_state_bytes() const noexcept {
    return stats_.peak_accounted_algorithm_state_bytes;
  }
};

}  // namespace ades
