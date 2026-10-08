#pragma once
#include "ades/baselines.hpp"
#include "ades/memory_budget.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ades {
// Independent, deterministic LRU comparator; original AlwaysResident is unchanged.
// Capacity bounds resident source states. When byte_budget is nonzero, the
// common PR43 logical persistent-state budget is enforced as an additional hard
// limit.
class BoundedResident {
 struct Entry { SSSPState state; std::list<std::uint32_t>::iterator recency; };
 Graph graph_;
 ResidentMode mode_;
 std::size_t capacity_;
 std::uint64_t byte_budget_=0;
 std::uint64_t peak_accounted_bytes_=0;
 std::list<std::uint32_t> lru_; // front = most recently queried
 std::unordered_map<std::uint32_t,Entry> states_;
 std::uint64_t hits_=0, misses_=0, evictions_=0;

 std::uint64_t accounted_bytes_impl()const{
  std::uint64_t bytes=0;
  for(const auto& [source,entry]:states_){
   (void)source;
   bytes=accounted_add(bytes,bounded_resident_entry_accounted_bytes(entry.state));
  }
  return bytes;
 }
 void note_peak(){
  peak_accounted_bytes_=std::max(peak_accounted_bytes_,accounted_bytes_impl());
  if(byte_budget_&&peak_accounted_bytes_>byte_budget_)
   throw std::logic_error("bounded comparator exceeded persistent-state byte budget");
 }
 void evict_lru(){
  if(lru_.empty())throw std::logic_error("cannot evict from empty bounded residency");
  auto victim=lru_.back();
  states_.erase(victim);
  lru_.pop_back();
  ++evictions_;
 }
 void enforce_budget(){
  if(!byte_budget_)return;
  while(!states_.empty()&&accounted_bytes_impl()>byte_budget_)evict_lru();
  note_peak();
 }
public:
 BoundedResident(Graph g,ResidentMode mode,std::size_t capacity,
                 std::uint64_t byte_budget=0)
     :graph_(std::move(g)),mode_(mode),capacity_(capacity),byte_budget_(byte_budget){
  if(!capacity_)throw std::invalid_argument("bounded residency capacity must be positive");
 }
 std::size_t resident_count()const noexcept{return states_.size();}
 std::size_t capacity()const noexcept{return capacity_;}
 std::uint64_t persistent_state_budget_bytes()const noexcept{return byte_budget_;}
 std::uint64_t accounted_algorithm_state_bytes()const{return accounted_bytes_impl();}
 std::uint64_t peak_accounted_algorithm_state_bytes()const noexcept{return peak_accounted_bytes_;}
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
  // The newly built state is temporary until insertion and therefore is not
  // charged to the persistent-state budget.
  auto state=dijkstra(graph_,source);
  auto answer=state.dist.at(target);
  ++misses_;
  const auto candidate_bytes=bounded_resident_entry_accounted_bytes(state);
  if(byte_budget_&&candidate_bytes>byte_budget_)return answer;

  while(states_.size()>=capacity_||
        (byte_budget_&&accounted_add(accounted_bytes_impl(),candidate_bytes)>byte_budget_)){
   if(states_.empty())return answer;
   evict_lru();
  }

  lru_.push_front(source);
  try{states_.emplace(source,Entry{std::move(state),lru_.begin()});}
  catch(...){lru_.pop_front();throw;}
  note_peak();
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
  enforce_budget();
 }
};
} // namespace ades
