#include "ades/ades.hpp"
namespace ades {
void ADES::rebuild(std::uint32_t s){auto state=dijkstra(graph_,s); residents_[s]=Entry{std::move(state),0};stats_.rebuilds++;}
Distance ADES::query(std::uint32_t s,std::uint32_t t){
 if(auto it=residents_.find(s);it!=residents_.end()){stats_.resident_queries++;it->second.hits++;return it->second.s.dist.at(t);}
 stats_.cold_queries++; auto ans=bidirectional_dijkstra(graph_,s,t);
 auto &p=probation_[s]; if(++p>=cfg_.probation_queries && residents_.size()<cfg_.resident_cap){rebuild(s);stats_.promotions++;probation_.erase(s);}
 return ans;
}
void ADES::update(std::uint32_t id,Weight nw){
 auto old=graph_.edge(id); if(old.weight==nw)return; graph_.update_weight(id,nw);
 for(auto &kv:residents_){
  auto &st=kv.second.s;
  auto r=nw<old.weight?repair_decrease(graph_,st,id):repair_increase(graph_,st,id,old,cfg_.repair_budget);
  if(r==RepairResult::Filtered){stats_.filtered_updates++;continue;}
  if(r==RepairResult::Repaired){if(nw<old.weight)stats_.decrease_repairs++;else stats_.increase_repairs++;continue;}
  stats_.repair_aborts++;st=dijkstra(graph_,st.source);stats_.rebuilds++;
 }
}
}
