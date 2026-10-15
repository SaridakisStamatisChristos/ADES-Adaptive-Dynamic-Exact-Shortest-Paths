#include "ades/dynamic_repair.hpp"
#include "ades/shortest_paths.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <random>

using namespace ades;

static void strict_alternate_witness_rescues_after_large_discovery_trigger(){
 Graph g(1024);
 g.add_edge(0,1,1);
 const auto changed=g.add_edge(0,2,2);
 const auto alternate=g.add_edge(1,2,1);
 for(std::uint32_t v=2;v+1<g.vertex_count();++v)g.add_edge(v,v+1,1);
 auto state=dijkstra(g,0);
 assert(state.parent_edge[2]==changed);
 const auto before_dist=state.dist;
 const auto old=g.edge(changed);g.update_weight(changed,3);
 RepairWork work{};std::size_t discovered=0;
 const auto result=repair_increase(g,state,changed,old,{2000,2000,std::numeric_limits<std::uint64_t>::max()},&discovered,&work);
 assert(result==RepairResult::Repaired);
 assert(work.witness_rescues==1&&work.witness_scans>0);
 assert(discovered==256);
 assert(state.parent_edge[2]==alternate);
 assert(state.dist==before_dist);
 assert(state.dist==dijkstra(g,0).dist);
}

static void small_repairs_do_not_pay_witness_probe(){
 Graph g(64);
 g.add_edge(0,1,1);
 const auto changed=g.add_edge(0,2,2);
 g.add_edge(1,2,1);
 for(std::uint32_t v=2;v+1<g.vertex_count();++v)g.add_edge(v,v+1,1);
 auto state=dijkstra(g,0);
 assert(state.parent_edge[2]==changed);
 const auto old=g.edge(changed);g.update_weight(changed,3);
 RepairWork work{};std::size_t discovered=0;
 const auto result=repair_increase(g,state,changed,old,{1000,1000,std::numeric_limits<std::uint64_t>::max()},&discovered,&work);
 assert(result==RepairResult::Repaired);
 assert(discovered==62);
 assert(work.witness_scans==0&&work.witness_rescues==0);
 assert(state.dist==dijkstra(g,0).dist);
}

static void equal_distance_zero_weight_witness_rescues_after_full_discovery(){
 Graph g(512);
 g.add_edge(0,1,0);g.add_edge(0,2,0);
 const auto via1=g.add_edge(1,3,0);
 const auto via2=g.add_edge(2,3,0);
 for(std::uint32_t v=3;v+1<g.vertex_count();++v)g.add_edge(v,v+1,1);
 auto state=dijkstra(g,0);
 const auto selected=state.parent_edge[3];
 assert(selected==via1||selected==via2);
 const auto alternate=selected==via1?via2:via1;
 const auto before_dist=state.dist;
 const auto old=g.edge(selected);g.update_weight(selected,1);
 RepairWork work{};std::size_t discovered=0;
 const auto result=repair_increase(g,state,selected,old,{1000,1000,std::numeric_limits<std::uint64_t>::max()},&discovered,&work);
 assert(result==RepairResult::Repaired);
 assert(discovered==509);
 assert(work.witness_scans>0&&work.witness_rescues==1);
 assert(state.parent_edge[3]==alternate);
 assert(state.dist==before_dist);
 assert(state.dist==dijkstra(g,0).dist);
}

static void equal_distance_descendant_is_not_a_safe_witness(){
 Graph g(512);
 const auto changed=g.add_edge(0,1,1);
 g.add_edge(1,2,0);
 g.add_edge(2,1,0);
 for(std::uint32_t v=2;v+1<g.vertex_count();++v)g.add_edge(v,v+1,1);
 auto state=dijkstra(g,0);
 assert(state.parent_edge[1]==changed);
 const auto old=g.edge(changed);g.update_weight(changed,2);
 RepairWork work{};std::size_t discovered=0;
 const auto result=repair_increase(g,state,changed,old,{1000,1000,std::numeric_limits<std::uint64_t>::max()},&discovered,&work);
 assert(result==RepairResult::Repaired);
 assert(discovered==511);
 assert(work.witness_scans>0&&work.witness_rescues==0);
 assert(state.dist==dijkstra(g,0).dist);
 assert(state.dist[1]==2&&state.dist[2]==2);
}

static void randomized_selected_parent_increase_differential(){
 for(std::uint64_t seed=0;seed<40;++seed){
  std::mt19937_64 rng(seed);Graph base(24);
  for(int i=0;i<240;++i){
   const auto u=std::uint32_t(rng()%24),v=std::uint32_t(rng()%24);
   if(u!=v)base.add_edge(u,v,Weight(rng()%8));
  }
  for(std::uint32_t source=0;source<24;++source){
   const auto original=dijkstra(base,source);int checked=0;
   for(std::uint32_t v=0;v<24&&checked<8;++v){
    const auto id=original.parent_edge[v];if(id==kNoIndex)continue;
    Graph changed=base;auto state=original;const auto old=base.edge(id);
    changed.update_weight(id,old.weight+1+Weight(rng()%7));
    RepairWork work{};
    const auto result=repair_increase(changed,state,id,old,{1000000,1000000,std::numeric_limits<std::uint64_t>::max()},nullptr,&work);
    assert(result!=RepairResult::RebuildRequired);
    assert(work.witness_scans==0);
    assert(state.dist==dijkstra(changed,source).dist);
    ++checked;
   }
  }
 }
}

int main(){
 strict_alternate_witness_rescues_after_large_discovery_trigger();
 small_repairs_do_not_pay_witness_probe();
 equal_distance_zero_weight_witness_rescues_after_full_discovery();
 equal_distance_descendant_is_not_a_safe_witness();
 randomized_selected_parent_increase_differential();
}
