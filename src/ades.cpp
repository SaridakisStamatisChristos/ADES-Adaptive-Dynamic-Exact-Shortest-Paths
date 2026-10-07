#include "ades/ades.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
namespace ades {
using Clock=std::chrono::steady_clock;
static std::uint64_t ns_since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}
double ADES::resident_score(const Entry&e)const{
 const auto age=query_clock_>=e.last_query?query_clock_-e.last_query:0;
 return double(e.hits)/(1.0+double(age)+cfg_.eviction_update_penalty*double(e.update_debt));
}
void ADES::rebuild(std::uint32_t source){
 auto t=Clock::now();auto state=dijkstra(graph_,source);auto ns=ns_since(t);
 if(auto it=residents_.find(source);it!=residents_.end()){it->second.s=std::move(state);it->second.controller.observe_rebuild(ns);}
 else{Entry e;e.s=std::move(state);e.last_query=query_clock_;e.controller.observe_rebuild(ns);residents_.emplace(source,std::move(e));}
 stats_.rebuilds++;
}
bool ADES::admit(std::uint32_t source,double candidate_score){
 if(residents_.contains(source))return true;
 if(auto c=cooldown_until_.find(source);c!=cooldown_until_.end()&&query_clock_<c->second){stats_.cooldown_blocks++;return false;}
 if(cfg_.resident_cap==0)return false;
 if(residents_.size()>=cfg_.resident_cap){
  auto victim=residents_.begin();double worst=resident_score(victim->second);
  for(auto it=std::next(residents_.begin());it!=residents_.end();++it){auto score=resident_score(it->second);if(score<worst){worst=score;victim=it;}}
  if(candidate_score<cfg_.admission_hysteresis*worst){stats_.admission_rejections++;return false;}
  cooldown_until_[victim->first]=query_clock_+cfg_.cooldown_queries;residents_.erase(victim);stats_.evictions++;
 }
 rebuild(source);
 auto& admitted=residents_.at(source);admitted.hits=std::max<std::uint64_t>(1,(std::uint64_t)candidate_score);admitted.last_query=query_clock_;
 stats_.promotions++;return true;
}
Distance ADES::query(std::uint32_t s,std::uint32_t t){
 query_clock_++;
 if(auto it=residents_.find(s);it!=residents_.end()){stats_.resident_queries++;it->second.hits++;it->second.last_query=query_clock_;return it->second.s.dist.at(t);}
 stats_.cold_queries++;auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
 auto&p=probation_[s];p.queries++;p.edge_scans+=cold.edge_scans;const double build_work=double(graph_.edge_count());
 if(p.queries>=cfg_.probation_queries&&double(p.edge_scans)>=cfg_.promotion_ratio*build_work){
  const double candidate_score=double(p.queries);
  if(admit(s,candidate_score))probation_.erase(s);else{p.queries=0;p.edge_scans=0;}
 }
 return cold.distance;
}
void ADES::update(std::uint32_t id,Weight nw){
 auto old=graph_.edge(id);if(old.weight==nw)return;graph_.update_weight(id,nw);
 for(auto&kv:residents_){auto&entry=kv.second;entry.update_debt++;auto&st=entry.s;RepairResult r;std::size_t discovered=0;RepairWork work{};auto t=Clock::now();
  if(nw<old.weight)r=repair_decrease(graph_,st,id);
  else{auto b=entry.controller.budget();r=repair_increase(graph_,st,id,old,{std::min(b.vertices,cfg_.repair_safety_ceiling.max_discovery_vertices),std::min(b.tree_edges,cfg_.repair_safety_ceiling.max_discovery_tree_edges)},&discovered,&work);}
  auto elapsed=ns_since(t);
  if(r==RepairResult::Filtered){stats_.filtered_updates++;continue;}
  if(r==RepairResult::Repaired){if(nw<old.weight)stats_.decrease_repairs++;else{stats_.increase_repairs++;entry.controller.observe_repair(elapsed,work);}continue;}
  stats_.repair_aborts++;t=Clock::now();st=dijkstra(graph_,st.source);entry.controller.observe_rebuild(ns_since(t));stats_.rebuilds++;
 }
}
}
