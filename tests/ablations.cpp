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

 const auto economic=config_for_scientific_ablation(ScientificAblation::PredictiveEconomic,base);
 require(economic.admission_policy==AdmissionPolicy::Economic,"economic admission mismatch");
 require(economic.eviction_policy==EvictionPolicy::Economic,"economic eviction mismatch");
 require(economic.maintenance_policy==MaintenancePolicy::LocalRepair,"economic maintenance mismatch");
 require(!economic.admission_hysteresis_enabled&&!economic.cooldown_enabled,"economic policy must use its own switching margin");
 require(std::string_view(scientific_ablation_name(ScientificAblation::PredictiveEconomic))=="ADES-V2","economic profile name mismatch");

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

 Config economic_fast=economic;
 economic_fast.economic_horizon_queries=1024; // force a clearly profitable repeated source in this tiny fixture.
 economic_fast.economic_replacement_margin=1.0;
 ADES economic_engine(graph(),economic_fast);
 const auto fixed_predictor_bytes=economic_engine.accounted_algorithm_state_bytes();
 require(fixed_predictor_bytes>0,"economic predictor must be charged to persistent-state accounting");
 require(economic_engine.query(0,4)==5,"economic first-query exactness failure");
 require(!economic_engine.resident(0),"economic policy promoted on first sight without reuse evidence");
 require(economic_engine.query(0,4)==5,"economic fused-promotion exactness failure");
 require(economic_engine.resident(0),"economic policy failed to promote repeated profitable source");
 require(economic_engine.stats().fused_promotions==1,"economic promotion was not recorded as fused");
 require(economic_engine.stats().promotions==1&&economic_engine.stats().rebuilds==1,"economic promotion build counters mismatch");
 require(economic_engine.stats().timing.reconciles(),"economic telemetry failed reconciliation");
 economic_engine.update(0,1);
 require(economic_engine.query(0,4)==4,"economic update exactness failure");
 require(economic_engine.stats().timing.reconciles(),"economic post-update telemetry failed reconciliation");

 ADES unique_engine(graph(),economic);
 const auto unique_before=unique_engine.accounted_algorithm_state_bytes();
 require(unique_engine.query(0,4)==5,"unique source 0 exactness failure");
 require(unique_engine.query(1,4)==3,"unique source 1 exactness failure");
 require(unique_engine.query(2,4)==4,"unique source 2 exactness failure");
 require(unique_engine.query(3,4)==2,"unique source 3 exactness failure");
 require(unique_engine.accounted_algorithm_state_bytes()==unique_before,
         "first-sight sources unexpectedly created unbounded economic metadata");
 require(unique_engine.resident_count()==0,"unique-source stream unexpectedly created residents");

 return 0;
}