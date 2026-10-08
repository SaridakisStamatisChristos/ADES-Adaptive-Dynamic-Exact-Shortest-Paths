#include "ades/ades.hpp"
#include "ades/memory_budget.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <vector>
namespace ades {
using Clock=std::chrono::steady_clock;
static std::uint64_t ns_since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}

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
 return bytes;
}

std::uint64_t ADES::accounted_algorithm_state_bytes_impl()const{
 std::uint64_t bytes=nonresident_accounted_bytes_impl();
 for(const auto& [source,entry]:residents_){
  (void)source;
  bytes=accounted_add(bytes,ades_resident_entry_accounted_bytes(entry.s));
 }
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
  return false;
 }
 return true;
}

bool ADES::admit(std::uint32_t source,double candidate_score){
 if(residents_.contains(source))return true;
 if(cfg_.admission_policy==AdmissionPolicy::Disabled)return false;
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
  return candidate_bytes>cfg_.persistent_state_budget_bytes-simulated;
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
  stats_.resident_queries++;it->second.hits++;it->second.last_query=query_clock_;
  const auto answer=it->second.s.dist.at(t);
  const auto total=ns_since(query_start);
  stats_.timing.query_time_ns+=total;
  stats_.timing.resident_query_ns+=total;
  return answer;
 }

 stats_.cold_queries++;
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
 auto graph_start=Clock::now();graph_.update_weight(id,nw);stats_.timing.graph_update_ns+=ns_since(graph_start);
 for(auto&kv:residents_){
  auto&entry=kv.second;
  if(cfg_.eviction_policy==EvictionPolicy::DebtAware)entry.update_debt++;
  auto&st=entry.s;
  if(cfg_.maintenance_policy==MaintenancePolicy::FullRebuild){
   auto rebuild_start=Clock::now();st=dijkstra(graph_,st.source);stats_.timing.rebuild_ns+=ns_since(rebuild_start);stats_.rebuilds++;
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
    continue;
   }
  }
  if(r==RepairResult::Filtered){stats_.filtered_updates++;continue;}
  if(r==RepairResult::Repaired){stats_.decrease_repairs++;continue;}
  stats_.repair_aborts++;
  auto rebuild_start=Clock::now();st=dijkstra(graph_,st.source);const auto rebuild_ns=ns_since(rebuild_start);stats_.timing.rebuild_ns+=rebuild_ns;
  auto controller_start=Clock::now();entry.controller.observe_rebuild(rebuild_ns);stats_.timing.controller_ns+=ns_since(controller_start);stats_.rebuilds++;
 }
 refresh_accounted_memory();
 finish_update();
}
}