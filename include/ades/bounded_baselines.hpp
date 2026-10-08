#pragma once
#include "ades/baselines.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ades {
// Independent, deterministic LRU comparator; original AlwaysResident is unchanged.
// The capacity bounds resident source states, not the total process RSS.
class BoundedResident {
 struct Entry { SSSPState state; std::list<std::uint32_t>::iterator recency; };
 Graph graph_;
 ResidentMode mode_;
 std::size_t capacity_;
 std::list<std::uint32_t> lru_; // front = most recently queried
 std::unordered_map<std::uint32_t,Entry> states_;
 std::uint64_t hits_=0, misses_=0, evictions_=0;
public:
 BoundedResident(Graph g,ResidentMode mode,std::size_t capacity)
     :graph_(std::move(g)),mode_(mode),capacity_(capacity){
  if(!capacity_)throw std::invalid_argument("bounded residency capacity must be positive");
 }
 std::size_t resident_count()const noexcept{return states_.size();}
 std::size_t capacity()const noexcept{return capacity_;}
 std::uint64_t hits()const noexcept{return hits_;}
 std::uint64_t misses()const noexcept{return misses_;}
 std::uint64_t evictions()const noexcept{return evictions_;}
 Distance query(std::uint32_t source,std::uint32_t target){
  auto it=states_.find(source);
  if(it!=states_.end()){
   ++hits_;
   lru_.splice(lru_.begin(),lru_,it->second.recency);
   return it->second.state.dist.at(target);
  }
  // Validate and build before eviction so failed queries preserve existing cache.
  auto state=dijkstra(graph_,source);
  auto answer=state.dist.at(target);
  ++misses_;
  if(states_.size()==capacity_){
   auto victim=lru_.back();
   states_.erase(victim);
   lru_.pop_back();
   ++evictions_;
  }
  lru_.push_front(source);
  try{states_.emplace(source,Entry{std::move(state),lru_.begin()});}
  catch(...){lru_.pop_front();throw;}
  return answer;
 }
 void update(std::uint32_t id,Weight weight){
  auto old=graph_.edge(id);
  if(old.weight==weight)return;
  graph_.update_weight(id,weight);
  for(auto& [source,entry]:states_){
   auto& state=entry.state;
   if(mode_==ResidentMode::FullRebuild){state=dijkstra(graph_,source);continue;}
   auto result=weight<old.weight
      ?repair_decrease(graph_,state,id)
      :repair_increase(graph_,state,id,old,{1u<<30,1u<<30});
   if(result==RepairResult::RebuildRequired)state=dijkstra(graph_,source);
  }
 }
};
} // namespace ades
