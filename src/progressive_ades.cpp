#include "ades/progressive_ades.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ades {
namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
      .count();
}

void ewma(double& current, double sample, double alpha) {
  current = current == 0.0 ? sample : (1.0 - alpha) * current + alpha * sample;
}

constexpr std::uint64_t partial_metadata_bytes() {
  // unordered-map source key + epoch/recency/observation counters + four EWMAs.
  return sizeof(std::uint32_t) + 3u * sizeof(std::uint64_t) +
         4u * sizeof(double);
}

constexpr std::uint64_t resident_metadata_bytes() {
  // map key + hit/recency/observation counters + three EWMAs + repair model.
  return sizeof(std::uint32_t) + 3u * sizeof(std::uint64_t) +
         3u * sizeof(double) + repair_controller_accounted_bytes();
}

}  // namespace

ProgressiveADES::ProgressiveADES(Graph graph, ProgressiveConfig config)
    : graph_(std::move(graph)), cfg_(config) {
  stats_.persistent_state_budget_bytes = cfg_.persistent_state_budget_bytes;
  refresh_accounted_memory();
}

std::uint64_t ProgressiveADES::partial_entry_bytes() const {
  return accounted_add(
      progressive_frontier_accounted_bytes_for_vertices(graph_.vertex_count()),
      partial_metadata_bytes());
}

std::uint64_t ProgressiveADES::resident_entry_bytes() const {
  return accounted_add(sssp_state_accounted_bytes_for_vertices(graph_.vertex_count()),
                       resident_metadata_bytes());
}

std::uint64_t ProgressiveADES::accounted_bytes_impl() const {
  std::uint64_t bytes =
      ades_economic_recent_table_accounted_bytes(kRecentSlots);
  bytes = accounted_add(
      bytes, accounted_mul(static_cast<std::uint64_t>(partials_.size()),
                           partial_entry_bytes()));
  for (const auto& [source, entry] : residents_) {
    (void)source;
    bytes = accounted_add(bytes, sssp_state_accounted_bytes(entry.state));
    bytes = accounted_add(bytes, resident_metadata_bytes());
  }
  return bytes;
}

void ProgressiveADES::refresh_accounted_memory() {
  const auto current = accounted_bytes_impl();
  stats_.accounted_algorithm_state_bytes = current;
  stats_.peak_accounted_algorithm_state_bytes =
      std::max(stats_.peak_accounted_algorithm_state_bytes, current);
  stats_.persistent_state_budget_bytes = cfg_.persistent_state_budget_bytes;
  if (cfg_.persistent_state_budget_bytes &&
      current > cfg_.persistent_state_budget_bytes)
    throw std::logic_error("ADES-V4 exceeded persistent-state byte budget");
}

bool ProgressiveADES::can_fit(std::uint64_t extra) const {
  if (!cfg_.persistent_state_budget_bytes) return true;
  const auto current = accounted_bytes_impl();
  return current <= cfg_.persistent_state_budget_bytes &&
         extra <= cfg_.persistent_state_budget_bytes - current;
}

void ProgressiveADES::remember(std::uint32_t source,
                               std::uint64_t last_query, double cold_ns,
                               double edge_scans) {
  auto& slot = recent_[source % kRecentSlots];
  slot.source = source;
  slot.last_query = last_query;
  slot.cold_ns = static_cast<std::uint64_t>(std::max(0.0, cold_ns));
  slot.edge_scans =
      static_cast<std::uint64_t>(std::max(0.0, edge_scans));
  slot.valid = true;
}

void ProgressiveADES::remember(const PartialEntry& entry) {
  remember(entry.frontier.source, entry.last_query, entry.cold_ns_ewma,
           entry.cold_edge_scans_ewma);
}

void ProgressiveADES::erase_partial(
    std::unordered_map<std::uint32_t, PartialEntry>::iterator it,
    bool invalidation, bool eviction) {
  remember(it->second);
  partials_.erase(it);
  if (invalidation) ++stats_.partial_invalidations;
  if (eviction) ++stats_.partial_evictions;
  refresh_accounted_memory();
}

void ProgressiveADES::prune_stale_partials() {
  bool changed = false;
  for (auto it = partials_.begin(); it != partials_.end();) {
    if (it->second.graph_epoch == graph_epoch_) {
      ++it;
      continue;
    }
    remember(it->second);
    it = partials_.erase(it);
    ++stats_.partial_invalidations;
    changed = true;
  }
  if (changed) refresh_accounted_memory();
}

bool ProgressiveADES::ensure_partial_room(std::uint64_t required,
                                          std::uint32_t protected_source) {
  if (!cfg_.partial_cap) return false;
  if (cfg_.persistent_state_budget_bytes &&
      required > cfg_.persistent_state_budget_bytes)
    return false;

  prune_stale_partials();
  auto needs_reclaim = [&] {
    const bool cap_full = partials_.size() >= cfg_.partial_cap;
    return cap_full || !can_fit(required);
  };

  while (needs_reclaim()) {
    auto victim = partials_.end();
    for (auto it = partials_.begin(); it != partials_.end(); ++it) {
      if (it->first == protected_source) continue;
      if (victim == partials_.end() ||
          it->second.last_query < victim->second.last_query ||
          (it->second.last_query == victim->second.last_query &&
           it->first < victim->first))
        victim = it;
    }
    if (victim == partials_.end()) return false;
    erase_partial(victim, false, true);
  }
  return true;
}

double ProgressiveADES::estimate_full_build_ns(const PartialEntry& entry) const {
  const double cold = std::max(1.0, entry.cold_ns_ewma);
  const double scans = std::max(1.0, entry.cold_edge_scans_ewma);
  if (!graph_.edge_count()) return cold;
  const double ratio =
      std::clamp(double(graph_.edge_count()) / scans, 1.0, 64.0);
  return cold * ratio;
}

double ProgressiveADES::partial_promotion_value(
    const PartialEntry& entry) const {
  if (entry.observations < cfg_.full_min_observations ||
      entry.reuse_gap_ewma <= 0.0 || entry.partial_query_ns_ewma <= 0.0)
    return -std::numeric_limits<double>::infinity();

  const double horizon =
      double(std::max<std::uint64_t>(1, cfg_.horizon_queries));
  const double gap = std::max(1.0, entry.reuse_gap_ewma);
  const double predicted_reuses = std::clamp(horizon / gap, 0.0, horizon);
  const double full_build = estimate_full_build_ns(entry);
  const double progress = graph_.edge_count()
                              ? std::clamp(double(entry.frontier.forward_edge_scans) /
                                               double(graph_.edge_count()),
                                           0.0, 1.0)
                              : 1.0;
  const double remaining_build = full_build * (1.0 - progress);
  const double update_rate =
      query_clock_ ? double(update_clock_) / double(query_clock_) : 0.0;
  // Before direct maintenance evidence exists, use one measured cold query as a
  // conservative repair-risk proxy rather than pretending updates are free.
  const double maintenance = maintenance_ns_ewma_ > 0.0
                                 ? maintenance_ns_ewma_
                                 : std::max(1.0, entry.cold_ns_ewma);
  const double expected_maintenance = horizon * update_rate * maintenance;
  return predicted_reuses * entry.partial_query_ns_ewma - remaining_build -
         expected_maintenance;
}

double ProgressiveADES::resident_value(const ResidentEntry& entry) const {
  if (entry.cold_ns_ewma <= 0.0) return 0.0;
  const double horizon =
      double(std::max<std::uint64_t>(1, cfg_.horizon_queries));
  const double age = double(std::max<std::uint64_t>(
      1, query_clock_ >= entry.last_query ? query_clock_ - entry.last_query : 1));
  const double gap = std::max({1.0, entry.reuse_gap_ewma, age});
  const double predicted_reuses = std::clamp(horizon / gap, 0.0, horizon);
  const double update_rate =
      query_clock_ ? double(update_clock_) / double(query_clock_) : 0.0;
  const double maintenance = entry.maintenance_ns_ewma > 0.0
                                 ? entry.maintenance_ns_ewma
                                 : (maintenance_ns_ewma_ > 0.0
                                        ? maintenance_ns_ewma_
                                        : entry.cold_ns_ewma);
  return predicted_reuses * entry.cold_ns_ewma -
         horizon * update_rate * maintenance;
}

bool ProgressiveADES::should_promote(std::uint32_t source,
                                     double candidate_value) const {
  if (cfg_.resident_cap == 0 || !(candidate_value > 0.0)) return false;
  const auto candidate_bytes = resident_entry_bytes();
  if (cfg_.persistent_state_budget_bytes &&
      candidate_bytes > cfg_.persistent_state_budget_bytes)
    return false;

  auto simulated = accounted_bytes_impl();
  if (partials_.contains(source)) simulated -= partial_entry_bytes();
  auto resident_count = residents_.size();

  struct Victim {
    std::uint32_t source;
    double value;
    std::uint64_t bytes;
  };
  std::vector<Victim> victims;
  victims.reserve(residents_.size());
  for (const auto& [resident, entry] : residents_)
    victims.push_back(
        {resident, resident_value(entry),
         accounted_add(sssp_state_accounted_bytes(entry.state),
                       resident_metadata_bytes())});
  std::sort(victims.begin(), victims.end(), [](const Victim& a, const Victim& b) {
    return a.value < b.value || (a.value == b.value && a.source < b.source);
  });

  std::size_t next = 0;
  auto needs_eviction = [&] {
    if (resident_count >= cfg_.resident_cap) return true;
    if (!cfg_.persistent_state_budget_bytes) return false;
    return simulated > cfg_.persistent_state_budget_bytes ||
           candidate_bytes > cfg_.persistent_state_budget_bytes - simulated;
  };

  while (needs_eviction()) {
    if (next >= victims.size()) return false;
    const auto& victim = victims[next++];
    if (candidate_value <=
        cfg_.replacement_margin * std::max(0.0, victim.value))
      return false;
    simulated -= victim.bytes;
    --resident_count;
  }
  return true;
}

bool ProgressiveADES::admit_completed(
    std::uint32_t source, SSSPState state, std::uint64_t completion_ns,
    double candidate_value, std::uint64_t observations, double reuse_gap,
    double cold_ns, double estimated_full_build_ns) {
  (void)completion_ns;
  if (!should_promote(source, candidate_value)) return false;

  auto partial = partials_.find(source);
  if (partial == partials_.end())
    throw std::logic_error("ADES-V4 promotion lost source partial");
  partials_.erase(partial);
  refresh_accounted_memory();

  const auto candidate_bytes = accounted_add(
      sssp_state_accounted_bytes(state), resident_metadata_bytes());

  struct Victim {
    std::uint32_t source;
    double value;
  };
  std::vector<Victim> victims;
  victims.reserve(residents_.size());
  for (const auto& [resident, entry] : residents_)
    victims.push_back({resident, resident_value(entry)});
  std::sort(victims.begin(), victims.end(), [](const Victim& a, const Victim& b) {
    return a.value < b.value || (a.value == b.value && a.source < b.source);
  });

  std::size_t next = 0;
  auto needs_eviction = [&] {
    if (residents_.size() >= cfg_.resident_cap) return true;
    return !can_fit(candidate_bytes);
  };

  const auto eviction_start = Clock::now();
  while (needs_eviction()) {
    if (next >= victims.size())
      throw std::logic_error("ADES-V4 promotion feasibility changed unexpectedly");
    const auto victim = victims[next++].source;
    const auto& entry = residents_.at(victim);
    remember(victim, entry.last_query, entry.cold_ns_ewma, 0.0);
    residents_.erase(victim);
    ++stats_.evictions;
    refresh_accounted_memory();
  }
  if (next) stats_.timing.eviction_ns += elapsed_ns(eviction_start);

  ResidentEntry entry;
  entry.state = std::move(state);
  entry.hits = 1;
  entry.last_query = query_clock_;
  entry.observations = std::max<std::uint64_t>(1, observations);
  entry.reuse_gap_ewma = reuse_gap;
  entry.cold_ns_ewma = cold_ns;
  entry.controller.observe_rebuild(static_cast<std::uint64_t>(
      std::max(1.0, estimated_full_build_ns)));
  residents_.emplace(source, std::move(entry));
  ++stats_.promotions;
  ++stats_.progressive_promotions;
  ++stats_.rebuilds;
  refresh_accounted_memory();
  return true;
}

void ProgressiveADES::record_maintenance(ResidentEntry& entry,
                                         std::uint64_t ns) {
  ewma(entry.maintenance_ns_ewma, double(ns), cfg_.ewma_alpha);
  ewma(maintenance_ns_ewma_, double(ns), cfg_.ewma_alpha);
}

Distance ProgressiveADES::query(std::uint32_t source, std::uint32_t target) {
  const auto query_start = Clock::now();
  const auto cold_before = stats_.timing.cold_query_ns;
  const auto promotion_before = stats_.timing.promotion_ns;
  const auto eviction_before = stats_.timing.eviction_ns;
  auto finish_nonresident = [&](Distance answer) {
    const auto total = elapsed_ns(query_start);
    const auto components = (stats_.timing.cold_query_ns - cold_before) +
                            (stats_.timing.promotion_ns - promotion_before) +
                            (stats_.timing.eviction_ns - eviction_before);
    if (components > total)
      throw std::logic_error("ADES-V4 query telemetry overlap");
    stats_.timing.query_time_ns += total;
    stats_.timing.query_policy_ns += total - components;
    return answer;
  };

  ++query_clock_;
  if (auto it = residents_.find(source); it != residents_.end()) {
    const auto gap = query_clock_ >= it->second.last_query
                         ? query_clock_ - it->second.last_query
                         : 1;
    ewma(it->second.reuse_gap_ewma,
         double(std::max<std::uint64_t>(1, gap)), cfg_.ewma_alpha);
    ++it->second.observations;
    ++it->second.hits;
    it->second.last_query = query_clock_;
    ++stats_.resident_queries;
    const auto answer = it->second.state.dist.at(target);
    const auto total = elapsed_ns(query_start);
    stats_.timing.query_time_ns += total;
    stats_.timing.resident_query_ns += total;
    return answer;
  }

  ++stats_.cold_queries;
  auto partial = partials_.find(source);
  if (partial != partials_.end() && partial->second.graph_epoch != graph_epoch_) {
    erase_partial(partial, true, false);
    partial = partials_.end();
  }

  if (partial != partials_.end()) {
    auto& entry = partial->second;
    const auto gap = query_clock_ >= entry.last_query
                         ? query_clock_ - entry.last_query
                         : 1;
    ewma(entry.reuse_gap_ewma, double(std::max<std::uint64_t>(1, gap)),
         cfg_.ewma_alpha);
    entry.last_query = query_clock_;
    ++entry.observations;

    if (entry.observations >= cfg_.full_min_observations) {
      ++stats_.economic_candidates;
      const auto candidate_value = partial_promotion_value(entry);
      if (should_promote(source, candidate_value)) {
        const auto observations = entry.observations;
        const auto reuse_gap = entry.reuse_gap_ewma;
        const auto cold_ns = entry.cold_ns_ewma;
        const auto full_build = estimate_full_build_ns(entry);
        const auto promotion_start = Clock::now();
        auto state = complete_progressive_sssp(graph_, entry.frontier);
        const auto completion_ns = elapsed_ns(promotion_start);
        stats_.timing.promotion_ns += completion_ns;
        const auto answer = state.dist.at(target);
        if (!admit_completed(source, std::move(state), completion_ns,
                             candidate_value, observations, reuse_gap, cold_ns,
                             full_build))
          throw std::logic_error("ADES-V4 accepted then rejected promotion");
        return finish_nonresident(answer);
      }
      ++stats_.economic_rejections;
    }

    const auto progressive_start = Clock::now();
    const auto result =
        progressive_bidirectional_query(graph_, entry.frontier, target);
    const auto progressive_ns = elapsed_ns(progressive_start);
    stats_.timing.cold_query_ns += progressive_ns;
    ++stats_.progressive_queries;
    ewma(entry.partial_query_ns_ewma, double(progressive_ns), cfg_.ewma_alpha);
    if (result.became_complete) ++stats_.partial_completions;
    return finish_nonresident(result.distance);
  }

  auto& slot = recent_[source % kRecentSlots];
  if (slot.valid && slot.source == source && slot.last_query < query_clock_ &&
      slot.cold_ns > 0) {
    const auto gap = query_clock_ - slot.last_query;
    if (gap <= std::max<std::uint64_t>(1, cfg_.horizon_queries)) {
      const auto bytes = partial_entry_bytes();
      if (ensure_partial_room(bytes)) {
        PartialEntry created;
        created.frontier = ProgressiveFrontier(graph_.vertex_count(), source);
        created.graph_epoch = graph_epoch_;
        created.last_query = query_clock_;
        created.observations = 2;
        created.reuse_gap_ewma = double(gap);
        created.cold_ns_ewma = double(slot.cold_ns);
        created.cold_edge_scans_ewma = double(slot.edge_scans);
        auto [it, inserted] = partials_.emplace(source, std::move(created));
        if (!inserted) throw std::logic_error("ADES-V4 duplicate partial source");
        ++stats_.partial_creations;
        refresh_accounted_memory();

        const auto progressive_start = Clock::now();
        const auto result =
            progressive_bidirectional_query(graph_, it->second.frontier, target);
        const auto progressive_ns = elapsed_ns(progressive_start);
        stats_.timing.cold_query_ns += progressive_ns;
        ++stats_.progressive_queries;
        ewma(it->second.partial_query_ns_ewma, double(progressive_ns),
             cfg_.ewma_alpha);
        if (result.became_complete) ++stats_.partial_completions;
        return finish_nonresident(result.distance);
      }
      ++stats_.memory_budget_rejections;
    }
  }

  const auto cold_start = Clock::now();
  const auto cold = bidirectional_dijkstra_profiled(graph_, source, target);
  const auto cold_ns = elapsed_ns(cold_start);
  stats_.timing.cold_query_ns += cold_ns;
  remember(source, query_clock_, double(cold_ns), double(cold.edge_scans));
  return finish_nonresident(cold.distance);
}

void ProgressiveADES::update(std::uint32_t edge_id, Weight new_weight) {
  const auto update_start = Clock::now();
  const auto graph_before = stats_.timing.graph_update_ns;
  const auto decrease_before = stats_.timing.decrease_repair_ns;
  const auto increase_before = stats_.timing.increase_repair_ns;
  const auto rebuild_before = stats_.timing.rebuild_ns;
  const auto controller_before = stats_.timing.controller_ns;
  auto finish = [&] {
    const auto total = elapsed_ns(update_start);
    const auto components = (stats_.timing.graph_update_ns - graph_before) +
                            (stats_.timing.decrease_repair_ns - decrease_before) +
                            (stats_.timing.increase_repair_ns - increase_before) +
                            (stats_.timing.rebuild_ns - rebuild_before) +
                            (stats_.timing.controller_ns - controller_before);
    if (components > total)
      throw std::logic_error("ADES-V4 update telemetry overlap");
    stats_.timing.update_time_ns += total;
    stats_.timing.update_policy_ns += total - components;
  };

  const auto old = graph_.edge(edge_id);
  if (old.weight == new_weight) {
    finish();
    return;
  }

  const auto graph_start = Clock::now();
  graph_.update_weight(edge_id, new_weight);
  stats_.timing.graph_update_ns += elapsed_ns(graph_start);
  ++update_clock_;
  ++graph_epoch_;  // O(1) invalidation of every speculative partial frontier.

  for (auto& [source, entry] : residents_) {
    const auto resident_start = Clock::now();
    auto& state = entry.state;
    RepairResult result;
    RepairWork work{};
    std::size_t discovered = 0;

    if (new_weight < old.weight) {
      const auto repair_start = Clock::now();
      result = repair_decrease(graph_, state, edge_id);
      stats_.timing.decrease_repair_ns += elapsed_ns(repair_start);
    } else {
      const auto controller_start = Clock::now();
      const auto budget = entry.controller.budget();
      stats_.timing.controller_ns += elapsed_ns(controller_start);
      const RepairBudget bounded{
          std::min(budget.vertices,
                   cfg_.repair_safety_ceiling.max_discovery_vertices),
          std::min(budget.tree_edges,
                   cfg_.repair_safety_ceiling.max_discovery_tree_edges),
          std::min(budget.work_units,
                   cfg_.repair_safety_ceiling.max_total_work)};
      const auto repair_start = Clock::now();
      result = repair_increase(graph_, state, edge_id, old, bounded, &discovered,
                               &work);
      const auto repair_ns = elapsed_ns(repair_start);
      stats_.timing.increase_repair_ns += repair_ns;
      if (result == RepairResult::Repaired) {
        ++stats_.increase_repairs;
        const auto controller_observe = Clock::now();
        entry.controller.observe_repair(repair_ns, work);
        stats_.timing.controller_ns += elapsed_ns(controller_observe);
        record_maintenance(entry, elapsed_ns(resident_start));
        continue;
      }
    }

    if (result == RepairResult::Filtered) {
      ++stats_.filtered_updates;
      record_maintenance(entry, elapsed_ns(resident_start));
      continue;
    }
    if (result == RepairResult::Repaired) {
      ++stats_.decrease_repairs;
      record_maintenance(entry, elapsed_ns(resident_start));
      continue;
    }

    ++stats_.repair_aborts;
    const auto rebuild_start = Clock::now();
    state = dijkstra(graph_, state.source);
    const auto rebuild_ns = elapsed_ns(rebuild_start);
    stats_.timing.rebuild_ns += rebuild_ns;
    const auto controller_observe = Clock::now();
    entry.controller.observe_rebuild(rebuild_ns);
    stats_.timing.controller_ns += elapsed_ns(controller_observe);
    ++stats_.rebuilds;
    record_maintenance(entry, elapsed_ns(resident_start));
    (void)source;
  }

  refresh_accounted_memory();
  finish();
}

}  // namespace ades
