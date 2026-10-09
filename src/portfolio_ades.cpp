#include "ades/portfolio_ades.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ades {
namespace {
constexpr std::uint64_t portfolio_entry_metadata_bytes() {
  return sizeof(std::uint32_t) + 2u * sizeof(std::uint64_t);
}
constexpr std::uint64_t portfolio_recent_slot_bytes() {
  return sizeof(std::uint32_t) + sizeof(std::uint64_t) + sizeof(std::uint8_t);
}
}  // namespace

PortfolioADES::PortfolioADES(Graph graph, HazardConfig config)
    : graph_(std::move(graph)), cfg_(config) {
  eager_mode_ = graph_favors_eager();
  if (!eager_mode_) {
    fallback_ = std::make_unique<HazardAwareADES>(std::move(graph_), cfg_);
  } else {
    refresh_memory();
  }
}

std::uint64_t PortfolioADES::recent_bytes() const {
  return accounted_mul(kRecentSlots, portfolio_recent_slot_bytes());
}

std::uint64_t PortfolioADES::entry_bytes() const {
  return accounted_add(sssp_state_accounted_bytes_for_vertices(graph_.vertex_count()),
                       portfolio_entry_metadata_bytes());
}

std::uint64_t PortfolioADES::accounted_bytes() const {
  return accounted_add(recent_bytes(),
                       accounted_mul(static_cast<std::uint64_t>(states_.size()),
                                     entry_bytes()));
}

void PortfolioADES::refresh_memory() {
  if (!eager_mode_) return;
  const auto current = accounted_bytes();
  stats_.accounted_algorithm_state_bytes = current;
  stats_.peak_accounted_algorithm_state_bytes =
      std::max(stats_.peak_accounted_algorithm_state_bytes, current);
  if (cfg_.persistent_state_budget_bytes &&
      current > cfg_.persistent_state_budget_bytes)
    throw std::logic_error("portfolio ADES exceeded persistent-state budget");
}

bool PortfolioADES::can_fit_entry() const {
  if (!cfg_.persistent_state_budget_bytes) return true;
  const auto current = accounted_bytes();
  const auto bytes = entry_bytes();
  return current <= cfg_.persistent_state_budget_bytes &&
         bytes <= cfg_.persistent_state_budget_bytes - current;
}

bool PortfolioADES::graph_favors_eager() const {
  if (!cfg_.resident_cap || !graph_.vertex_count()) return false;
  const double mean_degree =
      double(graph_.edge_count()) / double(graph_.vertex_count());
  std::size_t max_degree = 0;
  for (std::uint32_t v = 0; v < graph_.vertex_count(); ++v)
    max_degree = std::max(max_degree, graph_.out(v).size());

  // This is a cost-shape classifier, not a dataset classifier: low average
  // branching keeps complete SSSP construction relatively close to a point
  // query, while a large hub is a strong warning against eager materialization.
  if (mean_degree > 4.5 || max_degree > 32) return false;

  if (!cfg_.persistent_state_budget_bytes) return true;
  const auto full_set = accounted_add(
      accounted_mul(static_cast<std::uint64_t>(cfg_.resident_cap),
                    accounted_add(
                        sssp_state_accounted_bytes_for_vertices(graph_.vertex_count()),
                        portfolio_entry_metadata_bytes())),
      accounted_mul(kRecentSlots, portfolio_recent_slot_bytes()));
  return full_set <= cfg_.persistent_state_budget_bytes;
}

std::unordered_map<std::uint32_t, PortfolioADES::Entry>::iterator
PortfolioADES::probation_victim() {
  auto victim = states_.end();
  for (auto it = states_.begin(); it != states_.end(); ++it) {
    if (it->second.hits != 0) continue;
    if (victim == states_.end() ||
        it->second.last_query < victim->second.last_query ||
        (it->second.last_query == victim->second.last_query &&
         it->first < victim->first))
      victim = it;
  }
  return victim;
}

void PortfolioADES::remember(std::uint32_t source) {
  auto& slot = recent_[source % kRecentSlots];
  slot.source = source;
  slot.last_query = query_clock_;
  slot.valid = true;
}

Distance PortfolioADES::query(std::uint32_t source, std::uint32_t target) {
  if (!eager_mode_) return fallback_->query(source, target);
  ++query_clock_;

  if (auto it = states_.find(source); it != states_.end()) {
    ++stats_.resident_queries;
    ++it->second.hits;
    it->second.last_query = query_clock_;
    return it->second.state.dist.at(target);
  }

  ++stats_.cold_queries;
  if (states_.size() < cfg_.resident_cap && can_fit_entry()) {
    auto state = dijkstra(graph_, source);
    const auto answer = state.dist.at(target);
    states_.emplace(source, Entry{std::move(state), 0, query_clock_});
    ++stats_.promotions;
    ++stats_.rebuilds;
    refresh_memory();
    return answer;
  }

  auto& slot = recent_[source % kRecentSlots];
  const bool repeated = slot.valid && slot.source == source &&
                        slot.last_query < query_clock_ &&
                        query_clock_ - slot.last_query <= cfg_.horizon_queries;
  if (repeated) {
    auto victim = probation_victim();
    if (victim != states_.end()) {
      auto state = dijkstra(graph_, source);
      const auto answer = state.dist.at(target);
      states_.erase(victim);
      ++stats_.evictions;
      states_.emplace(source, Entry{std::move(state), 0, query_clock_});
      ++stats_.promotions;
      ++stats_.rebuilds;
      refresh_memory();
      return answer;
    }
  }

  const auto answer = bidirectional_dijkstra(graph_, source, target);
  remember(source);
  return answer;
}

void PortfolioADES::update(std::uint32_t edge_id, Weight new_weight) {
  if (!eager_mode_) {
    fallback_->update(edge_id, new_weight);
    return;
  }
  const auto old = graph_.edge(edge_id);
  if (old.weight == new_weight) return;
  graph_.update_weight(edge_id, new_weight);
  for (auto& [source, entry] : states_) {
    auto& state = entry.state;
    const auto result = new_weight < old.weight
                            ? repair_decrease(graph_, state, edge_id)
                            : repair_increase(graph_, state, edge_id, old,
                                              {1u << 30, 1u << 30});
    if (result == RepairResult::RebuildRequired) {
      state = dijkstra(graph_, source);
      ++stats_.rebuilds;
    }
  }
  refresh_memory();
}

std::uint64_t PortfolioADES::accounted_algorithm_state_bytes() const {
  return eager_mode_ ? accounted_bytes()
                     : fallback_->accounted_algorithm_state_bytes();
}

std::uint64_t PortfolioADES::peak_accounted_algorithm_state_bytes() const {
  return eager_mode_ ? stats_.peak_accounted_algorithm_state_bytes
                     : fallback_->peak_accounted_algorithm_state_bytes();
}

}  // namespace ades
