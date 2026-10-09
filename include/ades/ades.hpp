#pragma once
#include "ades/dynamic_repair.hpp"
#include "ades/repair_controller.hpp"
#include "ades/telemetry.hpp"
#include <array>
#include <cstdint>
#include <unordered_map>
namespace ades {
enum class RepairPolicy { Fixed, VertexOnly, WorkAware };
enum class AdmissionPolicy { Disabled, Frequency, WorkAware, Economic };
enum class EvictionPolicy { LRU, DebtAware, Economic };
enum class MaintenancePolicy { FullRebuild, LocalRepair };
enum class ScientificAblation {
 Cold,
 FreqLruRebuild,
 FreqLruRepair,
 WorkLruRepair,
 WorkDebtRepair,
 FullADES,
 PredictiveEconomic,
};
struct Config {
 std::size_t resident_cap=4; std::uint32_t probation_queries=4; double promotion_ratio=1.05;
 std::uint64_t cooldown_queries=32; double eviction_update_penalty=4.0; double admission_hysteresis=1.10;
 RepairBudget repair_safety_ceiling{1u<<20,1u<<22}; RepairPolicy repair_policy=RepairPolicy::WorkAware;
 AdmissionPolicy admission_policy=AdmissionPolicy::WorkAware;
 EvictionPolicy eviction_policy=EvictionPolicy::DebtAware;
 MaintenancePolicy maintenance_policy=MaintenancePolicy::LocalRepair;
 bool admission_hysteresis_enabled=true;
 bool cooldown_enabled=true;
 // PR50 predictive-economic controller. These fields are ignored by all
 // pre-PR50 profiles, preserving the historical ADES-v1 semantics.
 double economic_ewma_alpha=0.25;
 std::uint32_t economic_min_observations=2;
 std::uint64_t economic_horizon_queries=16;
 double economic_replacement_margin=1.05;
 // Zero preserves legacy source-count-only behavior. Nonzero enables the PR43
 // common logical persistent-state byte budget in addition to resident_cap.
 std::uint64_t persistent_state_budget_bytes=0;
};
Config config_for_scientific_ablation(ScientificAblation profile,Config base={});
const char* scientific_ablation_name(ScientificAblation profile) noexcept;
struct Stats {
 std::uint64_t cold_queries=0,resident_queries=0,promotions=0,evictions=0,cooldown_blocks=0,rebuilds=0,
 admission_rejections=0,filtered_updates=0,decrease_repairs=0,increase_repairs=0,repair_aborts=0,
 memory_budget_rejections=0,memory_metadata_prunes=0,
 economic_candidates=0,economic_rejections=0,fused_promotions=0,
 accounted_algorithm_state_bytes=0,peak_accounted_algorithm_state_bytes=0,persistent_state_budget_bytes=0;
 TimingStats timing{};
};
class ADES {
 Graph graph_; Config cfg_; Stats stats_; std::uint64_t query_clock_=0,update_clock_=0;
 struct Entry { SSSPState s; std::uint64_t hits=0,last_query=0,update_debt=0; RepairController controller{}; };
 std::unordered_map<std::uint32_t,Entry> residents_;
 struct Probation { std::uint32_t queries=0; std::uint64_t edge_scans=0; };
 std::unordered_map<std::uint32_t,Probation> probation_;
 std::unordered_map<std::uint32_t,std::uint64_t> cooldown_until_;

 static constexpr std::size_t kEconomicRecentSlots=64;
 struct EconomicRecent {
  std::uint32_t source=0; std::uint64_t last_query=0,cold_ns=0,edge_scans=0; bool valid=false;
 };
 struct EconomicSource {
  std::uint64_t observations=0,last_query=0;
  double reuse_gap_ewma=0.0,cold_ns_ewma=0.0,edge_scans_ewma=0.0;
 };
 struct ResidentEconomic {
  std::uint64_t observations=0;
  double reuse_gap_ewma=0.0,cold_ns_ewma=0.0,maintenance_ns_ewma=0.0;
 };
 std::array<EconomicRecent,kEconomicRecentSlots> economic_recent_{};
 std::unordered_map<std::uint32_t,EconomicSource> economic_sources_;
 std::unordered_map<std::uint32_t,ResidentEconomic> resident_economics_;
 double economic_build_ns_ewma_=0.0,economic_maintenance_ns_ewma_=0.0;

 bool admit(std::uint32_t source,double candidate_score);
 bool admit_economic(std::uint32_t source,SSSPState state,std::uint64_t build_ns,double candidate_value);
 double resident_score(const Entry&)const;
 double economic_candidate_value(const EconomicSource&)const;
 double economic_resident_value(std::uint32_t,const Entry&)const;
 bool economic_should_promote(std::uint32_t,double)const;
 void economic_record_cold(std::uint32_t,std::uint64_t,std::uint64_t);
 void economic_record_maintenance(std::uint32_t,std::uint64_t);
 std::uint64_t accounted_algorithm_state_bytes_impl()const;
 std::uint64_t nonresident_accounted_bytes_impl()const;
 void refresh_accounted_memory();
 bool can_fit_accounted_bytes(std::uint64_t extra)const;
 void prune_expired_cooldowns();
 bool reclaim_nonresident_metadata_for(std::uint64_t required_bytes);
public:
 explicit ADES(Graph g,Config c={}):graph_(std::move(g)),cfg_(c){stats_.persistent_state_budget_bytes=cfg_.persistent_state_budget_bytes;refresh_accounted_memory();}
 Distance query(std::uint32_t s,std::uint32_t t);
 void update(std::uint32_t edge_id,Weight new_weight);
 const Graph& graph()const{return graph_;}
 const Stats& stats()const{return stats_;}
 const Config& config()const noexcept{return cfg_;}
 bool resident(std::uint32_t s)const{return residents_.contains(s);}
 std::size_t resident_count()const{return residents_.size();}
 std::uint64_t persistent_state_budget_bytes()const noexcept{return cfg_.persistent_state_budget_bytes;}
 std::uint64_t accounted_algorithm_state_bytes()const{return accounted_algorithm_state_bytes_impl();}
 std::uint64_t peak_accounted_algorithm_state_bytes()const noexcept{return stats_.peak_accounted_algorithm_state_bytes;}
};
}