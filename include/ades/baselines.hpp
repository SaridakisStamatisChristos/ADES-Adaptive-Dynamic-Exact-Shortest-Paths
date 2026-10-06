#pragma once
#include "ades/dynamic_repair.hpp"
#include <unordered_map>
namespace ades {
enum class ResidentMode { FullRebuild, LocalRepair };
class AlwaysResident {
 Graph graph_; ResidentMode mode_; std::unordered_map<std::uint32_t,SSSPState> states_;
public:
 AlwaysResident(Graph g,ResidentMode m):graph_(std::move(g)),mode_(m){}
 Distance query(std::uint32_t s,std::uint32_t t){
  auto it=states_.find(s);if(it==states_.end())it=states_.emplace(s,dijkstra(graph_,s)).first;return it->second.dist.at(t);
 }
 void update(std::uint32_t id,Weight nw){
  auto old=graph_.edge(id);if(old.weight==nw)return;graph_.update_weight(id,nw);
  for(auto&[source,st]:states_){
   if(mode_==ResidentMode::FullRebuild){st=dijkstra(graph_,source);continue;}
   auto r=nw<old.weight?repair_decrease(graph_,st,id):repair_increase(graph_,st,id,old,{1u<<30,1u<<30});
   if(r==RepairResult::RebuildRequired)st=dijkstra(graph_,source);
  }
 }
};
}
