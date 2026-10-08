#pragma once
#include "ades/dynamic_repair.hpp"
#include "ades/repair_controller.hpp"
#include "ades/telemetry.hpp"
#include <cstdint>
#include <unordered_map>
namespace ades {
enum class RepairPolicy { Fixed, VertexOnly, WorkAware };
enum class AdmissionPolicy { Disabled, Frequency, WorkAware };
enum class EvictionPolicy { LRU, DebtAware };
enum class MaintenancePolicy { FullRebuild, LocalRepair };
enum class ScientificAblation {
 Cold,
 FreqLruRebuild,
 FreqLruRepair,
 WorkLruRepair,
 WorkDebtRepair,
 FullADES,
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
 accounted_algorithm_state_bytes=0,peak_accounted_algorithm_state_bytes=0,persistent_state_budget_bytes=0;
 TimingStats timing{};
};
class ADES {
 Graph graph_; Config cfg_; Stats stats_; std::uint64_t query_clock_=0;
 struct Entry { SSSPState s; std::uint64_t hits=0,last_query=0,update_debt=0; RepairController controller{}; };
 std::unordered_map<std::uint32_t,Entry> residents_;
 struct Probation { std::uint32_t queries=0; std::uint64_t edge_scans=0; };
 std::unordered_map<std::uint32_t,Probation> probation_;
 std::unordered_map<std::uint32_t,std::uint64_t> cooldown_until_;
 bool admit(std::uint32_t source,double candidate_score);
 double resident_score(const Entry&)const;
 std::uint64_t accounted_algorithm_state_bytes_impl()const;
 std::uint64_t nonresident_accounted_bytes_impl()const;
 void refresh_accounted_memory();
 bool can_fit_accounted_bytes(std::uint64_t extra)const;
 void prune_expired_cooldowns();
 bool reclaim_nonresident_metadata_for(std::uint64_t required_bytes);
public:
 explicit ADES(Graph g,Config c={}):graph_(std::move(g)),cfg_(c){stats_.persistent_state_budget_bytes=cfg_.persistent_state_budget_bytes;}
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