#pragma once

#include "ades/progressive_frontier.hpp"
#include "ades/repair_controller.hpp"
#include "ades/telemetry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace ades {

struct ProgressiveConfig {
  std::size_t resident_cap = 4;
  std::size_t partial_cap = 8;
  std::uint64_t persistent_state_budget_bytes = 0;
  std::uint64_t horizon_queries = 16;
  std::uint32_t full_min_observations = 3;
  double ewma_alpha = 0.25;
  double replacement_margin = 1.05;
  RepairBudget repair_safety_ceiling{1u << 20, 1u << 22};
};

struct ProgressiveStats {
  std::uint64_t cold_queries = 0;
  std::uint64_t resident_queries = 0;
  std::uint64_t progressive_queries = 0;
  std::uint64_t promotions = 0;
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
  std::uint64_t accounted_algorithm_state_bytes = 0;
  std::uint64_t peak_accounted_algorithm_state_bytes = 0;
  std::uint64_t persistent_state_budget_bytes = 0;
  TimingStats timing{};
};

class ProgressiveADES {
  Graph graph_;
  ProgressiveConfig cfg_;
  ProgressiveStats stats_;
  std::uint64_t query_clock_ = 0;
  std::uint64_t update_clock_ = 0;
  std::uint64_t graph_epoch_ = 0;

  static constexpr std::size_t kRecentSlots = 64;
  struct Recent {
    std::uint32_t source = 0;
    std::uint64_t last_query = 0;
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
  double maintenance_ns_ewma_ = 0.0;

  std::uint64_t partial_entry_bytes() const;
  std::uint64_t resident_entry_bytes() const;
  std::uint64_t accounted_bytes_impl() const;
  void refresh_accounted_memory();
  bool can_fit(std::uint64_t extra) const;
  void remember(std::uint32_t source, std::uint64_t last_query,
                double cold_ns, double edge_scans);
  void remember(const PartialEntry& entry);
  void erase_partial(std::unordered_map<std::uint32_t, PartialEntry>::iterator it,
                     bool invalidation, bool eviction);
  void prune_stale_partials();
  bool ensure_partial_room(std::uint64_t required,
                           std::uint32_t protected_source = UINT32_MAX);
  double estimate_full_build_ns(const PartialEntry& entry) const;
  double partial_promotion_value(const PartialEntry& entry) const;
  double resident_value(const ResidentEntry& entry) const;
  bool should_promote(std::uint32_t source, double candidate_value) const;
  bool admit_completed(std::uint32_t source, SSSPState state,
                       std::uint64_t completion_ns, double candidate_value,
                       std::uint64_t observations, double reuse_gap,
                       double cold_ns, double estimated_full_build_ns);
  void record_maintenance(ResidentEntry& entry, std::uint64_t ns);

 public:
  explicit ProgressiveADES(Graph graph, ProgressiveConfig config = {});

  Distance query(std::uint32_t source, std::uint32_t target);
  void update(std::uint32_t edge_id, Weight new_weight);

  const Graph& graph() const noexcept { return graph_; }
  const ProgressiveConfig& config() const noexcept { return cfg_; }
  const ProgressiveStats& stats() const noexcept { return stats_; }
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
