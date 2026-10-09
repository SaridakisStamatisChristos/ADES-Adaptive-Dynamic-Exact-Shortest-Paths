#include "ades/ades.hpp"
#include "ades/memory_budget.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace ades {
using Clock=std::chrono::steady_clock;
static std::uint64_t ns_since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}
static void ewma(double& current,double sample,double alpha){
 current=current==0.0?sample:(1.0-alpha)*current+alpha*sample;
}
static bool economic_policy(AdmissionPolicy policy){
 return policy==AdmissionPolicy::Economic||policy==AdmissionPolicy::EconomicFast;
}

Config config_for_scientific_ablation(ScientificAblation profile,Config base){
 base.admission_hysteresis_enabled=false;
 base.cooldown_enabled=false;
 switch(profile){
  case ScientificAblation::Cold:
   base.resident_cap=0;
   base.admission_policy=AdmissionPolicy::Disabled;
   base.eviction_policy=EvictionPolicy::LRU;
   base.maintenance_policy=MaintenancePolicy::FullRebuild;
   break;
  case ScientificAblation::FreqLruRebuild:
   base.admission_policy=AdmissionPolicy::Frequency;
   base.eviction_policy=EvictionPolicy::LRU;
   base.maintenance_policy=MaintenancePolicy::FullRebuild;
   break;
  case ScientificAblation::FreqLruRepair:
   base.admission_policy=AdmissionPolicy::Frequency;
   base.eviction_policy=EvictionPolicy::LRU;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   break;
  case ScientificAblation::WorkLruRepair:
   base.admission_policy=AdmissionPolicy::WorkAware;
   base.eviction_policy=EvictionPolicy::LRU;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   break;
  case ScientificAblation::WorkDebtRepair:
   base.admission_policy=AdmissionPolicy::WorkAware;
   base.eviction_policy=EvictionPolicy::DebtAware;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   break;
  case ScientificAblation::FullADES:
   base.admission_policy=AdmissionPolicy::WorkAware;
   base.eviction_policy=EvictionPolicy::DebtAware;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   base.admission_hysteresis_enabled=true;
   base.cooldown_enabled=true;
   break;
  case ScientificAblation::PredictiveEconomic:
   base.admission_policy=AdmissionPolicy::Economic;
   base.eviction_policy=EvictionPolicy::Economic;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   break;
  case ScientificAblation::PredictiveEconomicFast:
   base.admission_policy=AdmissionPolicy::EconomicFast;
   base.eviction_policy=EvictionPolicy::Economic;
   base.maintenance_policy=MaintenancePolicy::LocalRepair;
   base.economic_fast_nonresident=true;
   base.economic_horizon_queries=32;
   break;
 }
 return base;
}

const char* scientific_ablation_name(ScientificAblation profile) noexcept{
 switch(profile){
  case ScientificAblation::Cold:return "COLD";
  case ScientificAblation::FreqLruRebuild:return "FREQ-LRU-REBUILD";
  case ScientificAblation::FreqLruRepair:return "FREQ-LRU-REPAIR";
  case ScientificAblation::WorkLruRepair:return "WORK-LRU-REPAIR";
  case ScientificAblation::WorkDebtRepair:return "WORK-DEBT-REPAIR";
  case ScientificAblation::FullADES:return "B4";
  case ScientificAblation::PredictiveEconomic:return "ADES-V2";
  case ScientificAblation::PredictiveEconomicFast:return "ADES-V3";
 }
 return "UNKNOWN";
}

double ADES::resident_score(const Entry&e)const{
 const auto age=query_clock_>=e.last_query?query_clock_-e.last_query:0;
 return double(e.hits)/(1.0+double(age)+cfg_.eviction_update_penalty*double(e.update_debt));
}

std::uint64_t ADES::nonresident_accounted_bytes_impl()const{
 std::uint64_t bytes=0;
 bytes=accounted_add(bytes,accounted_mul(probation_.size(),ades_probation_entry_accounted_bytes()));
 bytes=accounted_add(bytes,accounted_mul(cooldown_until_.size(),ades_cooldown_entry_accounted_bytes()));
 if(economic_policy(cfg_.admission_policy)){
  bytes=accounted_add(bytes,ades_economic_recent_table_accounted_bytes(kEconomicRecentSlots));
  if(cfg_.admission_policy==AdmissionPolicy::Economic)
   bytes=accounted_add(bytes,accounted_mul(economic_sources_.size(),ades_economic_source_accounted_bytes()));
 }
 return bytes;
}

std::uint64_t ADES::accounted_algorithm_state_bytes_impl()const{
 std::uint64_t bytes=nonresident_accounted_bytes_impl();
 for(const auto& [source,entry]:residents_){
  (void)source;
  bytes=accounted_add(bytes,ades_resident_entry_accounted_bytes(entry.s));
 }
 if(economic_policy(cfg_.admission_policy))
  bytes=accounted_add(bytes,accounted_mul(resident_economics_.size(),ades_economic_resident_accounted_bytes()));
 return bytes;
}

void ADES::refresh_accounted_memory(){
 const auto current=accounted_algorithm_state_bytes_impl();
 stats_.accounted_algorithm_state_bytes=current;
 stats_.peak_accounted_algorithm_state_bytes=std::max(stats_.peak_accounted_algorithm_state_bytes,current);
 stats_.persistent_state_budget_bytes=cfg_.persistent_state_budget_bytes;
 if(cfg_.persistent_state_budget_bytes&&current>cfg_.persistent_state_budget_bytes)
  throw std::logic_error("ADES exceeded persistent-state byte budget");
}

bool ADES::can_fit_accounted_bytes(std::uint64_t extra)const{
 if(!cfg_.persistent_state_budget_bytes)return true;
 const auto current=accounted_algorithm_state_bytes_impl();
 return current<=cfg_.persistent_state_budget_bytes&&
        extra<=cfg_.persistent_state_budget_bytes-current;
}

void ADES::prune_expired_cooldowns(){
 if(!cfg_.cooldown_enabled){
  if(!cooldown_until_.empty()){cooldown_until_.clear();refresh_accounted_memory();}
  return;
 }
 bool changed=false;
 for(auto it=cooldown_until_.begin();it!=cooldown_until_.end();){
  if(query_clock_>=it->second){it=cooldown_until_.erase(it);changed=true;}
  else ++it;
 }
 if(changed)refresh_accounted_memory();
}

bool ADES::reclaim_nonresident_metadata_for(std::uint64_t required_bytes){
 if(!cfg_.persistent_state_budget_bytes)return true;
 auto fits=[&]{
  const auto nonresident=nonresident_accounted_bytes_impl();
  return nonresident<=cfg_.persistent_state_budget_bytes&&
         required_bytes<=cfg_.persistent_state_budget_bytes-nonresident;
 };
 while(!fits()){
  if(!probation_.empty()){
   auto victim=probation_.begin();
   for(auto it=probation_.begin();it!=probation_.end();++it){
    const auto& a=it->second;const auto& b=victim->second;
    if(a.queries<b.queries||
       (a.queries==b.queries&&a.edge_scans<b.edge_scans)||
       (a.queries==b.queries&&a.edge_scans==b.edge_scans&&it->first<victim->first))victim=it;
   }
   probation_.erase(victim);stats_.memory_metadata_prunes++;refresh_accounted_memory();continue;
  }
  if(!cooldown_until_.empty()){
   auto victim=cooldown_until_.begin();
   for(auto it=cooldown_until_.begin();it!=cooldown_until_.end();++it)
    if(it->second<victim->second||(it->second==victim->second&&it->first<victim->first))victim=it;
   cooldown_until_.erase(victim);stats_.memory_metadata_prunes++;refresh_accounted_memory();continue;
  }
  if(!economic_sources_.empty()){
   auto victim=economic_sources_.begin();
   for(auto it=economic_sources_.begin();it!=economic_sources_.end();++it){
    const auto& a=it->second;const auto& b=victim->second;
    if(a.observations<b.observations||
       (a.observations==b.observations&&a.last_query<b.last_query)||
       (a.observations==b.observations&&a.last_query==b.last_query&&it->first<victim->first))victim=it;
   }
   economic_sources_.erase(victim);stats_.memory_metadata_prunes++;refresh_accounted_memory();continue;
  }
  return false;
 }
 return true;
}

bool ADES::admit(std::uint32_t source,double candidate_score){
 if(residents_.contains(source))return true;
 if(cfg_.admission_policy==AdmissionPolicy::Disabled||economic_policy(cfg_.admission_policy))return false;
 if(cfg_.cooldown_enabled){
  if(auto c=cooldown_until_.find(source);c!=cooldown_until_.end()&&query_clock_<c->second){stats_.cooldown_blocks++;return false;}
 }
 if(cfg_.resident_cap==0)return false;

 const auto candidate_bytes=ades_resident_entry_accounted_bytes_for_vertices(graph_.vertex_count());
 if(cfg_.persistent_state_budget_bytes&&candidate_bytes>cfg_.persistent_state_budget_bytes){
  stats_.memory_budget_rejections++;return false;
 }
 if(!reclaim_nonresident_metadata_for(candidate_bytes)){
  stats_.memory_budget_rejections++;return false;
 }

 struct Victim{std::uint32_t source;double score;std::uint64_t bytes;std::uint64_t last_query;};
 std::vector<Victim> candidates;candidates.reserve(residents_.size());
 for(const auto& [s,e]:residents_)
  candidates.push_back({s,resident_score(e),ades_resident_entry_accounted_bytes(e.s),e.last_query});
 if(cfg_.eviction_policy==EvictionPolicy::LRU){
  std::sort(candidates.begin(),candidates.end(),[](const Victim&a,const Victim&b){
   return a.last_query<b.last_query||(a.last_query==b.last_query&&a.source<b.source);
  });
 }else{
  std::sort(candidates.begin(),candidates.end(),[](const Victim&a,const Victim&b){
   return a.score<b.score||(a.score==b.score&&a.source<b.source);
  });
 }

 auto simulated=accounted_algorithm_state_bytes_impl();
 auto simulated_count=residents_.size();
 std::vector<std::uint32_t> victims;
 std::size_t next=0;
 auto needs_eviction=[&]{
  if(simulated_count>=cfg_.resident_cap)return true;
  if(!cfg_.persistent_state_budget_bytes)return false;
  return simulated>cfg_.persistent_state_budget_bytes||candidate_bytes>cfg_.persistent_state_budget_bytes-simulated;
 };
 while(needs_eviction()){
  if(next>=candidates.size()){stats_.memory_budget_rejections++;return false;}
  const auto& victim=candidates[next++];
  if(cfg_.admission_hysteresis_enabled&&candidate_score<cfg_.admission_hysteresis*victim.score){
   stats_.admission_rejections++;return false;
  }
  simulated-=victim.bytes;--simulated_count;victims.push_back(victim.source);
 }

 if(!victims.empty()){
  auto t=Clock::now();
  for(auto victim:victims){residents_.erase(victim);stats_.evictions++;}
  stats_.timing.eviction_ns+=ns_since(t);
 }

 auto promotion_start=Clock::now();
 auto state=dijkstra(graph_,source);
 const auto build_ns=ns_since(promotion_start);
 if(ades_resident_entry_accounted_bytes(state)!=candidate_bytes)
  throw std::logic_error("unexpected SSSP accounting shape");
 Entry e;e.s=std::move(state);e.hits=std::max<std::uint64_t>(1,(std::uint64_t)candidate_score);e.last_query=query_clock_;
 if(cfg_.maintenance_policy==MaintenancePolicy::LocalRepair)e.controller.observe_rebuild(build_ns);
 residents_.emplace(source,std::move(e));stats_.rebuilds++;stats_.promotions++;refresh_accounted_memory();
 stats_.timing.promotion_ns+=ns_since(promotion_start);

 if(cfg_.cooldown_enabled&&!victims.empty()&&cfg_.cooldown_queries){
  auto t=Clock::now();
  for(auto victim:victims){
   auto existing=cooldown_until_.find(victim);
   if(existing!=cooldown_until_.end()){existing->second=query_clock_+cfg_.cooldown_queries;continue;}
   if(can_fit_accounted_bytes(ades_cooldown_entry_accounted_bytes()))
    cooldown_until_.emplace(victim,query_clock_+cfg_.cooldown_queries);
   else stats_.memory_budget_rejections++;
  }
  refresh_accounted_memory();
  stats_.timing.eviction_ns+=ns_since(t);
 }
 return true;
}

double ADES::economic_candidate_value(const EconomicSource& source)const{
 if(source.observations<cfg_.economic_min_observations||source.reuse_gap_ewma<=0.0||source.cold_ns_ewma<=0.0)
  return -std::numeric_limits<double>::infinity();
 const double horizon=double(std::max<std::uint64_t>(1,cfg_.economic_horizon_queries));
 const double gap=std::max(1.0,source.reuse_gap_ewma);
 const double predicted_reuses=std::clamp(horizon/gap,0.0,horizon);
 double build=economic_build_ns_ewma_;
 if(build<=0.0){
  const double scans=std::max(1.0,source.edge_scans_ewma);
  const double work_ratio=std::clamp(double(graph_.edge_count())/scans,1.0,64.0);
  build=source.cold_ns_ewma*work_ratio;
 }
 const double update_rate=query_clock_?double(update_clock_)/double(query_clock_):0.0;
 const double expected_maintenance=horizon*update_rate*economic_maintenance_ns_ewma_;
 return predicted_reuses*source.cold_ns_ewma-build-expected_maintenance;
}

double ADES::economic_resident_value(std::uint32_t source,const Entry& entry)const{
 auto it=resident_economics_.find(source);
 if(it==resident_economics_.end())return 0.0;
 const auto& economic=it->second;
 if(economic.cold_ns_ewma<=0.0)return 0.0;
 const double horizon=double(std::max<std::uint64_t>(1,cfg_.economic_horizon_queries));
 const double age=double(std::max<std::uint64_t>(1,query_clock_>=entry.last_query?query_clock_-entry.last_query:1));
 const double gap=std::max({1.0,economic.reuse_gap_ewma,age});
 const double predicted_reuses=std::clamp(horizon/gap,0.0,horizon);
 const double update_rate=query_clock_?double(update_clock_)/double(query_clock_):0.0;
 const double maintenance=economic.maintenance_ns_ewma>0.0?economic.maintenance_ns_ewma:economic_maintenance_ns_ewma_;
 return predicted_reuses*economic.cold_ns_ewma-horizon*update_rate*maintenance;
}

bool ADES::economic_should_promote(std::uint32_t source,double candidate_value)const{
 if(!economic_policy(cfg_.admission_policy)||cfg_.resident_cap==0||!(candidate_value>0.0))return false;
 const auto candidate_bytes=accounted_add(ades_resident_entry_accounted_bytes_for_vertices(graph_.vertex_count()),
                                          ades_economic_resident_accounted_bytes());
 if(cfg_.persistent_state_budget_bytes&&candidate_bytes>cfg_.persistent_state_budget_bytes)return false;
 auto simulated=accounted_algorithm_state_bytes_impl();
 if(cfg_.admission_policy==AdmissionPolicy::Economic&&economic_sources_.contains(source))
  simulated-=ades_economic_source_accounted_bytes();
 auto simulated_count=residents_.size();
 struct Victim{std::uint32_t source;double value;std::uint64_t bytes;};
 std::vector<Victim> candidates;candidates.reserve(residents_.size());
 for(const auto& [resident,entry]:residents_){
  auto bytes=ades_resident_entry_accounted_bytes(entry.s);
  if(resident_economics_.contains(resident))bytes=accounted_add(bytes,ades_economic_resident_accounted_bytes());
  candidates.push_back({resident,economic_resident_value(resident,entry),bytes});
 }
 std::sort(candidates.begin(),candidates.end(),[](const Victim&a,const Victim&b){
  return a.value<b.value||(a.value==b.value&&a.source<b.source);
 });
 std::size_t next=0;
 auto needs_eviction=[&]{
  if(simulated_count>=cfg_.resident_cap)return true;
  if(!cfg_.persistent_state_budget_bytes)return false;
  return simulated>cfg_.persistent_state_budget_bytes||candidate_bytes>cfg_.persistent_state_budget_bytes-simulated;
 };
 while(needs_eviction()){
  if(next>=candidates.size())return false;
  const auto& victim=candidates[next++];
  if(candidate_value<=cfg_.economic_replacement_margin*std::max(0.0,victim.value))return false;
  simulated-=victim.bytes;--simulated_count;
 }
 return true;
}

bool ADES::admit_economic(std::uint32_t source,SSSPState state,std::uint64_t build_ns,double candidate_value,const EconomicSource* history_override){
 if(residents_.contains(source))return true;
 if(!economic_should_promote(source,candidate_value)){stats_.economic_rejections++;return false;}
 const auto candidate_bytes=accounted_add(ades_resident_entry_accounted_bytes_for_vertices(graph_.vertex_count()),
                                          ades_economic_resident_accounted_bytes());
 struct Victim{std::uint32_t source;double value;std::uint64_t bytes;};
 std::vector<Victim> candidates;candidates.reserve(residents_.size());
 for(const auto& [resident,entry]:residents_){
  auto bytes=ades_resident_entry_accounted_bytes(entry.s);
  if(resident_economics_.contains(resident))bytes=accounted_add(bytes,ades_economic_resident_accounted_bytes());
  candidates.push_back({resident,economic_resident_value(resident,entry),bytes});
 }
 std::sort(candidates.begin(),candidates.end(),[](const Victim&a,const Victim&b){
  return a.value<b.value||(a.value==b.value&&a.source<b.source);
 });
 auto simulated=accounted_algorithm_state_bytes_impl();
 if(cfg_.admission_policy==AdmissionPolicy::Economic&&economic_sources_.contains(source))
  simulated-=ades_economic_source_accounted_bytes();
 auto simulated_count=residents_.size();
 std::vector<std::uint32_t> victims;
 std::size_t next=0;
 auto needs_eviction=[&]{
  if(simulated_count>=cfg_.resident_cap)return true;
  if(!cfg_.persistent_state_budget_bytes)return false;
  return simulated>cfg_.persistent_state_budget_bytes||candidate_bytes>cfg_.persistent_state_budget_bytes-simulated;
 };
 while(needs_eviction()){
  if(next>=candidates.size())return false;
  const auto& victim=candidates[next++];
  simulated-=victim.bytes;--simulated_count;victims.push_back(victim.source);
 }
 if(!victims.empty()){
  auto eviction_start=Clock::now();
  for(auto victim:victims){
   if(auto eit=resident_economics_.find(victim);eit!=resident_economics_.end()){
    auto& slot=economic_recent_[victim%kEconomicRecentSlots];
    slot.source=victim;slot.last_query=residents_.at(victim).last_query;
    slot.cold_ns=static_cast<std::uint64_t>(std::max(0.0,eit->second.cold_ns_ewma));
    slot.edge_scans=0;slot.valid=true;
    resident_economics_.erase(eit);
   }
   residents_.erase(victim);stats_.evictions++;
  }
  stats_.timing.eviction_ns+=ns_since(eviction_start);
 }
 EconomicSource history;
 if(history_override)history=*history_override;
 else if(auto it=economic_sources_.find(source);it!=economic_sources_.end())history=it->second;
 if(cfg_.admission_policy==AdmissionPolicy::Economic)economic_sources_.erase(source);
 Entry entry;entry.s=std::move(state);entry.hits=1;entry.last_query=query_clock_;
 if(cfg_.maintenance_policy==MaintenancePolicy::LocalRepair)entry.controller.observe_rebuild(build_ns);
 residents_.emplace(source,std::move(entry));
 ResidentEconomic economic;
 economic.observations=std::max<std::uint64_t>(1,history.observations);
 economic.reuse_gap_ewma=history.reuse_gap_ewma;
 economic.cold_ns_ewma=history.cold_ns_ewma;
 resident_economics_[source]=economic;
 ewma(economic_build_ns_ewma_,double(build_ns),cfg_.economic_ewma_alpha);
 stats_.rebuilds++;stats_.promotions++;stats_.fused_promotions++;
 refresh_accounted_memory();
 return true;
}

void ADES::economic_record_cold(std::uint32_t source,std::uint64_t cold_ns,std::uint64_t edge_scans){
 if(!economic_policy(cfg_.admission_policy))return;
 if(cfg_.admission_policy==AdmissionPolicy::Economic){
  if(auto it=economic_sources_.find(source);it!=economic_sources_.end()){
   ewma(it->second.cold_ns_ewma,double(cold_ns),cfg_.economic_ewma_alpha);
   ewma(it->second.edge_scans_ewma,double(edge_scans),cfg_.economic_ewma_alpha);
  }
 }
 auto& slot=economic_recent_[source%kEconomicRecentSlots];
 if(cfg_.admission_policy==AdmissionPolicy::EconomicFast&&slot.valid&&slot.source==source){
  double cold=double(slot.cold_ns),scans=double(slot.edge_scans);
  ewma(cold,double(cold_ns),cfg_.economic_ewma_alpha);
  ewma(scans,double(edge_scans),cfg_.economic_ewma_alpha);
  slot.cold_ns=static_cast<std::uint64_t>(cold);
  slot.edge_scans=static_cast<std::uint64_t>(scans);
 }else{
  slot.source=source;slot.cold_ns=cold_ns;slot.edge_scans=edge_scans;slot.valid=true;
 }
 slot.last_query=query_clock_;
}

void ADES::economic_record_maintenance(std::uint32_t source,std::uint64_t ns){
 if(!economic_policy(cfg_.admission_policy))return;
 if(auto it=resident_economics_.find(source);it!=resident_economics_.end())
  ewma(it->second.maintenance_ns_ewma,double(ns),cfg_.economic_ewma_alpha);
 ewma(economic_maintenance_ns_ewma_,double(ns),cfg_.economic_ewma_alpha);
}

Distance ADES::query(std::uint32_t s,std::uint32_t t){
 const auto query_start=Clock::now();
 const auto cold_before=stats_.timing.cold_query_ns;
 const auto promotion_before=stats_.timing.promotion_ns;
 const auto eviction_before=stats_.timing.eviction_ns;
 auto finish_cold=[&](Distance answer){
  const auto total=ns_since(query_start);
  const auto components=(stats_.timing.cold_query_ns-cold_before)+
                        (stats_.timing.promotion_ns-promotion_before)+
                        (stats_.timing.eviction_ns-eviction_before);
  if(components>total)throw std::logic_error("query telemetry overlap detected");
  stats_.timing.query_time_ns+=total;
  stats_.timing.query_policy_ns+=total-components;
  return answer;
 };

 query_clock_++;prune_expired_cooldowns();
 if(auto it=residents_.find(s);it!=residents_.end()){
  if(economic_policy(cfg_.admission_policy)){
   if(auto economic=resident_economics_.find(s);economic!=resident_economics_.end()){
    const auto gap=query_clock_>=it->second.last_query?query_clock_-it->second.last_query:1;
    ewma(economic->second.reuse_gap_ewma,double(std::max<std::uint64_t>(1,gap)),cfg_.economic_ewma_alpha);
    economic->second.observations++;
   }
  }
  stats_.resident_queries++;it->second.hits++;it->second.last_query=query_clock_;
  const auto answer=it->second.s.dist.at(t);
  const auto total=ns_since(query_start);
  stats_.timing.query_time_ns+=total;
  stats_.timing.resident_query_ns+=total;
  return answer;
 }

 stats_.cold_queries++;
 if(cfg_.admission_policy==AdmissionPolicy::EconomicFast){
  auto& slot=economic_recent_[s%kEconomicRecentSlots];
  if(slot.valid&&slot.source==s&&slot.last_query<query_clock_&&slot.cold_ns>0){
   EconomicSource candidate;
   candidate.observations=2;
   candidate.last_query=query_clock_;
   candidate.reuse_gap_ewma=double(query_clock_-slot.last_query);
   candidate.cold_ns_ewma=double(slot.cold_ns);
   candidate.edge_scans_ewma=double(slot.edge_scans);
   stats_.economic_candidates++;
   const auto candidate_value=economic_candidate_value(candidate);
   if(economic_should_promote(s,candidate_value)){
    auto promotion_start=Clock::now();
    auto state=dijkstra(graph_,s);
    const auto build_ns=ns_since(promotion_start);
    stats_.timing.promotion_ns+=build_ns;
    const auto answer=state.dist.at(t);
    if(admit_economic(s,std::move(state),build_ns,candidate_value,&candidate))return finish_cold(answer);
    return finish_cold(answer);
   }
   stats_.economic_rejections++;
  }
  auto cold_start=Clock::now();
  auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
  const auto cold_ns=ns_since(cold_start);
  stats_.timing.cold_query_ns+=cold_ns;
  economic_record_cold(s,cold_ns,cold.edge_scans);
  // The recent-source table has fixed accounted size; updating a slot cannot
  // change logical persistent bytes, so V3 deliberately skips a full accounting
  // traversal on the cold path.
  return finish_cold(cold.distance);
 }

 if(cfg_.admission_policy==AdmissionPolicy::Economic){
  auto history=economic_sources_.find(s);
  if(history!=economic_sources_.end()){
   const auto gap=query_clock_>=history->second.last_query?query_clock_-history->second.last_query:1;
   ewma(history->second.reuse_gap_ewma,double(std::max<std::uint64_t>(1,gap)),cfg_.economic_ewma_alpha);
   history->second.last_query=query_clock_;history->second.observations++;
  }else{
   const auto& slot=economic_recent_[s%kEconomicRecentSlots];
   if(slot.valid&&slot.source==s&&slot.last_query<query_clock_){
    if(can_fit_accounted_bytes(ades_economic_source_accounted_bytes())||
       reclaim_nonresident_metadata_for(ades_economic_source_accounted_bytes())){
     EconomicSource created;
     created.observations=2;created.last_query=query_clock_;
     created.reuse_gap_ewma=double(query_clock_-slot.last_query);
     created.cold_ns_ewma=double(slot.cold_ns);created.edge_scans_ewma=double(slot.edge_scans);
     history=economic_sources_.emplace(s,created).first;refresh_accounted_memory();
    }else stats_.memory_budget_rejections++;
   }
  }
  if(history!=economic_sources_.end()&&history->second.observations>=cfg_.economic_min_observations){
   stats_.economic_candidates++;
   const auto candidate_value=economic_candidate_value(history->second);
   if(economic_should_promote(s,candidate_value)){
    auto promotion_start=Clock::now();
    auto state=dijkstra(graph_,s);
    const auto build_ns=ns_since(promotion_start);
    stats_.timing.promotion_ns+=build_ns;
    const auto answer=state.dist.at(t);
    if(admit_economic(s,std::move(state),build_ns,candidate_value))return finish_cold(answer);
    return finish_cold(answer);
   }
   stats_.economic_rejections++;
  }
  auto cold_start=Clock::now();
  auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
  const auto cold_ns=ns_since(cold_start);
  stats_.timing.cold_query_ns+=cold_ns;
  economic_record_cold(s,cold_ns,cold.edge_scans);
  refresh_accounted_memory();
  return finish_cold(cold.distance);
 }

 auto cold_start=Clock::now();
 auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
 stats_.timing.cold_query_ns+=ns_since(cold_start);
 if(cfg_.admission_policy==AdmissionPolicy::Disabled)return finish_cold(cold.distance);

 auto pit=probation_.find(s);
 if(pit==probation_.end()){
  if(!can_fit_accounted_bytes(ades_probation_entry_accounted_bytes())){
   stats_.memory_budget_rejections++;return finish_cold(cold.distance);
  }
  pit=probation_.emplace(s,Probation{}).first;refresh_accounted_memory();
 }
 auto&p=pit->second;p.queries++;
 if(cfg_.admission_policy==AdmissionPolicy::WorkAware)p.edge_scans+=cold.edge_scans;
 const bool frequency_ready=p.queries>=cfg_.probation_queries;
 const bool work_ready=cfg_.admission_policy==AdmissionPolicy::Frequency||
                       double(p.edge_scans)>=cfg_.promotion_ratio*double(graph_.edge_count());
 if(frequency_ready&&work_ready){
  const double candidate_score=double(p.queries);
  probation_.erase(pit);refresh_accounted_memory();
  (void)admit(s,candidate_score);
 }
 return finish_cold(cold.distance);
}

void ADES::update(std::uint32_t id,Weight nw){
 const auto update_start=Clock::now();
 const auto graph_before=stats_.timing.graph_update_ns;
 const auto decrease_before=stats_.timing.decrease_repair_ns;
 const auto increase_before=stats_.timing.increase_repair_ns;
 const auto rebuild_before=stats_.timing.rebuild_ns;
 const auto controller_before=stats_.timing.controller_ns;
 auto finish_update=[&]{
  const auto total=ns_since(update_start);
  const auto components=(stats_.timing.graph_update_ns-graph_before)+
                        (stats_.timing.decrease_repair_ns-decrease_before)+
                        (stats_.timing.increase_repair_ns-increase_before)+
                        (stats_.timing.rebuild_ns-rebuild_before)+
                        (stats_.timing.controller_ns-controller_before);
  if(components>total)throw std::logic_error("update telemetry overlap detected");
  stats_.timing.update_time_ns+=total;
  stats_.timing.update_policy_ns+=total-components;
 };

 auto old=graph_.edge(id);
 if(old.weight==nw){finish_update();return;}
 auto graph_start=Clock::now();graph_.update_weight(id,nw);stats_.timing.graph_update_ns+=ns_since(graph_start);update_clock_++;
 for(auto&kv:residents_){
  const auto resident_update_start=Clock::now();
  auto&entry=kv.second;
  if(cfg_.eviction_policy==EvictionPolicy::DebtAware)entry.update_debt++;
  auto&st=entry.s;
  if(cfg_.maintenance_policy==MaintenancePolicy::FullRebuild){
   auto rebuild_start=Clock::now();st=dijkstra(graph_,st.source);stats_.timing.rebuild_ns+=ns_since(rebuild_start);stats_.rebuilds++;
   economic_record_maintenance(kv.first,ns_since(resident_update_start));
   continue;
  }

  RepairResult r;std::size_t discovered=0;RepairWork work{};
  if(nw<old.weight){
   auto t=Clock::now();r=repair_decrease(graph_,st,id);stats_.timing.decrease_repair_ns+=ns_since(t);
  }else{
   RepairController::Budget b{cfg_.repair_safety_ceiling.max_discovery_vertices,cfg_.repair_safety_ceiling.max_discovery_tree_edges,cfg_.repair_safety_ceiling.max_total_work};
   auto controller_start=Clock::now();
   if(cfg_.repair_policy==RepairPolicy::VertexOnly)b=entry.controller.vertex_only_budget();
   else if(cfg_.repair_policy==RepairPolicy::WorkAware)b=entry.controller.budget();
   stats_.timing.controller_ns+=ns_since(controller_start);
   auto repair_start=Clock::now();
   r=repair_increase(graph_,st,id,old,{std::min(b.vertices,cfg_.repair_safety_ceiling.max_discovery_vertices),std::min(b.tree_edges,cfg_.repair_safety_ceiling.max_discovery_tree_edges),std::min(b.work_units,cfg_.repair_safety_ceiling.max_total_work)},&discovered,&work);
   const auto repair_ns=ns_since(repair_start);stats_.timing.increase_repair_ns+=repair_ns;
   if(r==RepairResult::Repaired){
    stats_.increase_repairs++;
    controller_start=Clock::now();entry.controller.observe_repair(repair_ns,work);stats_.timing.controller_ns+=ns_since(controller_start);
    economic_record_maintenance(kv.first,ns_since(resident_update_start));
    continue;
   }
  }
  if(r==RepairResult::Filtered){stats_.filtered_updates++;economic_record_maintenance(kv.first,ns_since(resident_update_start));continue;}
  if(r==RepairResult::Repaired){stats_.decrease_repairs++;economic_record_maintenance(kv.first,ns_since(resident_update_start));continue;}
  stats_.repair_aborts++;
  auto rebuild_start=Clock::now();st=dijkstra(graph_,st.source);const auto rebuild_ns=ns_since(rebuild_start);stats_.timing.rebuild_ns+=rebuild_ns;
  auto controller_start=Clock::now();entry.controller.observe_rebuild(rebuild_ns);stats_.timing.controller_ns+=ns_since(controller_start);stats_.rebuilds++;
  economic_record_maintenance(kv.first,ns_since(resident_update_start));
 }
 refresh_accounted_memory();
 finish_update();
}
}