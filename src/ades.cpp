#include "ades/ades.hpp"
#include <algorithm>
#include <chrono>
namespace ades {
using Clock=std::chrono::steady_clock;
static std::uint64_t ns_since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}
void ADES::rebuild(std::uint32_t source){
 auto t=Clock::now();auto state=dijkstra(graph_,source);auto ns=ns_since(t);
 if(auto it=residents_.find(source);it!=residents_.end()){it->second.s=std::move(state);it->second.controller.observe_rebuild(ns);}
 else{Entry e;e.s=std::move(state);e.controller.observe_rebuild(ns);residents_.emplace(source,std::move(e));}
 stats_.rebuilds++;
}
Distance ADES::query(std::uint32_t s,std::uint32_t t){
 if(auto it=residents_.find(s);it!=residents_.end()){stats_.resident_queries++;it->second.hits++;return it->second.s.dist.at(t);}
 stats_.cold_queries++;auto cold=bidirectional_dijkstra_profiled(graph_,s,t);
 auto&p=probation_[s];p.queries++;p.edge_scans+=cold.edge_scans;
 const double build_work=double(graph_.edge_count());
 if(p.queries>=cfg_.probation_queries&&double(p.edge_scans)>=cfg_.promotion_ratio*build_work&&residents_.size()<cfg_.resident_cap){rebuild(s);stats_.promotions++;probation_.erase(s);}
 return cold.distance;
}
void ADES::update(std::uint32_t id,Weight nw){
 auto old=graph_.edge(id);if(old.weight==nw)return;graph_.update_weight(id,nw);
 for(auto&kv:residents_){auto&entry=kv.second;auto&st=entry.s;RepairResult r;std::size_t discovered=0;auto t=Clock::now();
  if(nw<old.weight)r=repair_decrease(graph_,st,id);
  else{auto b=entry.controller.budget();r=repair_increase(graph_,st,id,old,{std::min(b.vertices,cfg_.repair_safety_ceiling.max_discovery_vertices),std::min(b.tree_edges,cfg_.repair_safety_ceiling.max_discovery_tree_edges)},&discovered);}
  auto elapsed=ns_since(t);
  if(r==RepairResult::Filtered){stats_.filtered_updates++;continue;}
  if(r==RepairResult::Repaired){if(nw<old.weight)stats_.decrease_repairs++;else{stats_.increase_repairs++;entry.controller.observe_repair(elapsed,discovered);}continue;}
  stats_.repair_aborts++;t=Clock::now();st=dijkstra(graph_,st.source);entry.controller.observe_rebuild(ns_since(t));stats_.rebuilds++;
 }
}
}
