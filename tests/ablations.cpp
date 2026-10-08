#include "ades/ades.hpp"

#include <stdexcept>
#include <string_view>

using namespace ades;

static void require(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}

static Graph graph(){
 Graph g(5);
 g.add_edge(0,1,2); // 0
 g.add_edge(1,2,2); // 1
 g.add_edge(2,3,2); // 2
 g.add_edge(3,4,2); // 3
 g.add_edge(0,4,20); // 4
 g.add_edge(1,4,3); // 5
 return g;
}

int main(){
 Config base;
 base.resident_cap=2;
 base.probation_queries=1;
 base.promotion_ratio=0.0;

 const auto cold=config_for_scientific_ablation(ScientificAblation::Cold,base);
 require(cold.admission_policy==AdmissionPolicy::Disabled,"cold admission must be disabled");
 require(cold.resident_cap==0,"cold must have zero resident capacity");
 require(!cold.admission_hysteresis_enabled&&!cold.cooldown_enabled,"cold stabilizers must be disabled");

 const auto freq_rebuild=config_for_scientific_ablation(ScientificAblation::FreqLruRebuild,base);
 require(freq_rebuild.admission_policy==AdmissionPolicy::Frequency,"frequency admission mismatch");
 require(freq_rebuild.eviction_policy==EvictionPolicy::LRU,"LRU mismatch");
 require(freq_rebuild.maintenance_policy==MaintenancePolicy::FullRebuild,"rebuild maintenance mismatch");

 const auto freq_repair=config_for_scientific_ablation(ScientificAblation::FreqLruRepair,base);
 require(freq_repair.admission_policy==AdmissionPolicy::Frequency,"frequency repair admission mismatch");
 require(freq_repair.eviction_policy==EvictionPolicy::LRU,"frequency repair eviction mismatch");
 require(freq_repair.maintenance_policy==MaintenancePolicy::LocalRepair,"repair maintenance mismatch");

 const auto work_lru=config_for_scientific_ablation(ScientificAblation::WorkLruRepair,base);
 require(work_lru.admission_policy==AdmissionPolicy::WorkAware,"work admission mismatch");
 require(work_lru.eviction_policy==EvictionPolicy::LRU,"work LRU mismatch");
 require(!work_lru.admission_hysteresis_enabled&&!work_lru.cooldown_enabled,"work LRU must isolate stabilizers");

 const auto work_debt=config_for_scientific_ablation(ScientificAblation::WorkDebtRepair,base);
 require(work_debt.admission_policy==AdmissionPolicy::WorkAware,"work-debt admission mismatch");
 require(work_debt.eviction_policy==EvictionPolicy::DebtAware,"debt-aware eviction mismatch");
 require(!work_debt.admission_hysteresis_enabled&&!work_debt.cooldown_enabled,"work-debt must isolate stabilizers");

 const auto full=config_for_scientific_ablation(ScientificAblation::FullADES,base);
 require(full.admission_policy==AdmissionPolicy::WorkAware,"full admission mismatch");
 require(full.eviction_policy==EvictionPolicy::DebtAware,"full eviction mismatch");
 require(full.maintenance_policy==MaintenancePolicy::LocalRepair,"full maintenance mismatch");
 require(full.admission_hysteresis_enabled&&full.cooldown_enabled,"full stabilizers missing");
 require(std::string_view(scientific_ablation_name(ScientificAblation::FreqLruRepair))=="FREQ-LRU-REPAIR","profile name mismatch");

 ADES cold_engine(graph(),cold);
 require(cold_engine.query(0,4)==5,"cold exactness failure");
 require(cold_engine.query(0,4)==5,"cold repeat exactness failure");
 require(cold_engine.resident_count()==0,"cold unexpectedly materialized state");
 require(cold_engine.accounted_algorithm_state_bytes()==0,"cold unexpectedly retained adaptive state");

 ADES rebuild_engine(graph(),freq_rebuild);
 require(rebuild_engine.query(0,4)==5,"rebuild profile exactness failure");
 require(rebuild_engine.resident(0),"frequency profile failed to promote");
 const auto rebuilds_after_promotion=rebuild_engine.stats().rebuilds;
 rebuild_engine.update(0,1);
 require(rebuild_engine.stats().rebuilds==rebuilds_after_promotion+1,"full-rebuild profile did not rebuild resident SSSP");
 require(rebuild_engine.query(0,4)==4,"rebuild profile update exactness failure");
 require(rebuild_engine.stats().timing.reconciles(),"rebuild profile telemetry failed reconciliation");

 ADES repair_engine(graph(),freq_repair);
 require(repair_engine.query(0,4)==5,"repair profile exactness failure");
 require(repair_engine.resident(0),"repair profile failed to promote");
 const auto repair_rebuilds=repair_engine.stats().rebuilds;
 repair_engine.update(0,1);
 require(repair_engine.stats().decrease_repairs==1,"repair profile did not use decrease repair");
 require(repair_engine.stats().rebuilds==repair_rebuilds,"repair profile rebuilt instead of repairing decrease");
 require(repair_engine.query(0,4)==4,"repair profile update exactness failure");
 require(repair_engine.stats().timing.reconciles(),"repair profile telemetry failed reconciliation");

 return 0;
}
