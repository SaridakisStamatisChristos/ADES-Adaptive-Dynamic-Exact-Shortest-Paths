#include "ades/ades.hpp"
#include "ades/memory_budget.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>
namespace ades {
using Clock=std::chrono::steady_clock;
static std::uint64_t ns_since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}

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
 if(auto c=cooldown_until_.find(source);c!=cooldown_until_.end()&&query_clock_<c->second){stats_.cooldown_blocks++;return false;}
 if(cfg_.resident_cap==0)return false;

 const auto candidate_bytes=ades_resident_entry_accounted_bytes_for_vertices(graph_.vertex_count());
 if(cfg_.persistent_state_budget_bytes&&candidate_bytes>cfg_.persistent_state_budget_bytes){
  stats_.memory_budget_rejections++;return false;
 }
 // Ensure that, even with all resident states removed, B4 policy metadata plus
 // the candidate can fit. Lowest-value probation records and then earliest
 // cooldown records are deterministically pruned under pressure.
 if(!reclaim_nonresident_metadata_for(candidate_bytes)){
  stats_.memory_budget_rejections++;return false;
 }

 struct Victim{std::uint32_t source;double score;std::uint64_t bytes;};
 std::vector<Victim> candidates;candidates.reserve(residents_.size());
 for(const auto& [s,e]:residents_)
  candidates.push_back({s,resident_score(e),ades_resident_entry_accounted_bytes(e.s)});
 std::sort(candidates.begin(),candidates.end(),[](const Victim&a,const Victim&b){
  return a.score<b.score||(a.score==b.score&&a.source<b.source);
 });

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
  if(candidate_score<cfg_.admission_hysteresis*victim.score){stats_.admission_rejections++;return false;}
  simulated-=victim.bytes;--simulated_count;victims.push_back(victim.source);
 }

 // Build before mutating residency. The candidate remains temporary until it is
 // inserted and therefore does not count as persistent state during construction.
 auto t=Clock::now();auto state=dijkstra(graph_,source);auto ns=ns_since(t);
 if(ades_resident_entry_accounted_bytes(state)!=candidate_bytes)
  throw std::logic_error("unexpected SSSP accounting shape");

 for(auto victim:victims){residents_.erase(victim);stats_.evictions++;}
 Entry e;e.s=std::move(state);e.hits=std::max<std::uint64_t>(1,(std::uint64_t)candidate_score);e.last_query=query_clock_;e.controller.observe_rebuild(ns);
 residents_.emplace(source,std::move(e));stats_.rebuilds++;stats_.promotions++;refresh_accounted_memory();

 // Cooldown is policy metadata, not correctness state. Preserve it when it fits;
 // under a saturated byte budget, dropping it is preferable to violating C3.
 for(auto victim:victims){
  auto existing=cooldown_until_.find(victim);
  if(existing!=cooldown_until_.end()){existing->second=query_clock_+cfg_.cooldown_queries;continue;}
  if(can_fit_accounted_bytes(ades_cooldown_entry_accounted_bytes()))
   cooldown_until_.emplace(victim,query_clock_+cfg_.cooldown_queries);
  else stats_.memory_budget_rejections++;
 }
 refresh_accounted_memory();
 return true;
}

Distance ADES::query(std::uint32_t s,std::uint32_t t){
 query_clock_++;prune_expired_cooldowns();
 if(auto it=residents_.find(s);it!=residents_.end()){stats_.resident_queries++;it->second.hits++;it->second.last_query=query_clock_;return it->second.s.dist.at(t);}
 stats_.cold_queries++;auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
 auto pit=probation_.find(s);
 if(pit==probation_.end()){
  if(!can_fit_accounted_bytes(ades_probation_entry_accounted_bytes())){stats_.memory_budget_rejections++;return cold.distance;}
  pit=probation_.emplace(s,Probation{}).first;refresh_accounted_memory();
 }
 auto&p=pit->second;p.queries++;p.edge_scans+=cold.edge_scans;const double build_work=double(graph_.edge_count());
 if(p.queries>=cfg_.probation_queries&&double(p.edge_scans)>=cfg_.promotion_ratio*build_work){
  const double candidate_score=double(p.queries);
  // Successful admission replaces probation state; erase it before budget
  // planning so the same bytes are not charged twice.
  probation_.erase(pit);refresh_accounted_memory();
  (void)admit(s,candidate_score);
 }
 return cold.distance;
}

void ADES::update(std::uint32_t id,Weight nw){
 auto old=graph_.edge(id);if(old.weight==nw)return;graph_.update_weight(id,nw);
 for(auto&kv:residents_){auto&entry=kv.second;entry.update_debt++;auto&st=entry.s;RepairResult r;std::size_t discovered=0;RepairWork work{};auto t=Clock::now();
  if(nw<old.weight)r=repair_decrease(graph_,st,id);
  else{
   RepairController::Budget b{cfg_.repair_safety_ceiling.max_discovery_vertices,cfg_.repair_safety_ceiling.max_discovery_tree_edges,cfg_.repair_safety_ceiling.max_total_work};
   if(cfg_.repair_policy==RepairPolicy::VertexOnly)b=entry.controller.vertex_only_budget();
   else if(cfg_.repair_policy==RepairPolicy::WorkAware)b=entry.controller.budget();
   r=repair_increase(graph_,st,id,old,{std::min(b.vertices,cfg_.repair_safety_ceiling.max_discovery_vertices),std::min(b.tree_edges,cfg_.repair_safety_ceiling.max_discovery_tree_edges),std::min(b.work_units,cfg_.repair_safety_ceiling.max_total_work)},&discovered,&work);
  }
  auto elapsed=ns_since(t);
  if(r==RepairResult::Filtered){stats_.filtered_updates++;continue;}
  if(r==RepairResult::Repaired){if(nw<old.weight)stats_.decrease_repairs++;else{stats_.increase_repairs++;entry.controller.observe_repair(elapsed,work);}continue;}
  stats_.repair_aborts++;t=Clock::now();st=dijkstra(graph_,st.source);entry.controller.observe_rebuild(ns_since(t));stats_.rebuilds++;
 }
 refresh_accounted_memory();
}
}
