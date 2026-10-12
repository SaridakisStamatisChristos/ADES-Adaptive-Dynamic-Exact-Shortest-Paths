#include "ades/dynamic_repair.hpp"
#include "ades/memory_budget.hpp"
#include "ades/shortest_paths.hpp"

#include <cassert>
#include <cstdint>
#include <random>

using namespace ades;

static void tight_nonparent_increase_is_read_only() {
  Graph g(4);
  g.add_edge(0, 1, 1); g.add_edge(0, 2, 1); g.add_edge(1, 3, 1);
  const auto changed = g.add_edge(2, 3, 1);
  auto state = dijkstra(g, 0);
  assert(state.dist[3] == 2 && state.parent_edge[3] != changed);
  const auto before_dist=state.dist,before_parent=state.parent_edge,before_first=state.first_child,before_next=state.next_sibling,before_prev=state.prev_sibling;
  const auto old=g.edge(changed); g.update_weight(changed,100);
  RepairWork work{}; std::size_t discovered=99;
  assert(repair_increase(g,state,changed,old,{1,1},&discovered,&work)==RepairResult::Filtered);
  assert(discovered==0 && work.total()==0); assert(state.dist==before_dist); assert(state.parent_edge==before_parent);
  assert(state.first_child==before_first && state.next_sibling==before_next && state.prev_sibling==before_prev);
  assert(state.dist==dijkstra(g,0).dist);
}

static void parallel_tight_nonparent_increase_is_filtered() {
  Graph g(3); const auto selected=g.add_edge(0,1,1); const auto parallel=g.add_edge(0,1,1); g.add_edge(1,2,1);
  auto state=dijkstra(g,0); assert(state.parent_edge[1]==selected && state.parent_edge[1]!=parallel);
  const auto old=g.edge(parallel); g.update_weight(parallel,50);
  assert(repair_increase(g,state,parallel,old,{100,100})==RepairResult::Filtered); assert(state.dist==dijkstra(g,0).dist);
}

static void zero_weight_equal_witness_is_filtered() {
  Graph g(4); g.add_edge(0,1,0); g.add_edge(0,2,0); g.add_edge(1,3,1); const auto changed=g.add_edge(2,3,1);
  auto state=dijkstra(g,0); assert(state.parent_edge[3]!=changed); const auto old=g.edge(changed); g.update_weight(changed,2);
  assert(repair_increase(g,state,changed,old,{100,100})==RepairResult::Filtered); assert(state.dist==dijkstra(g,0).dist);
}

static void randomized_tight_nonparent_filter_differential() {
  for(std::uint64_t seed=0;seed<40;++seed){std::mt19937_64 rng(seed);Graph base(18);
    for(int i=0;i<150;++i){auto u=std::uint32_t(rng()%18),v=std::uint32_t(rng()%18);if(u!=v)base.add_edge(u,v,Weight(rng()%8));}
    for(std::uint32_t source=0;source<18;++source){const auto original=dijkstra(base,source);
      for(std::uint32_t id=0;id<base.edge_count();++id){const auto old=base.edge(id);if(original.dist[old.from]>=INF)continue;
        if(sat_add(original.dist[old.from],old.weight)!=original.dist[old.to]||original.parent_edge[old.to]==id)continue;
        Graph changed=base;auto state=original;changed.update_weight(id,old.weight+1+Weight(rng()%9));
        assert(repair_increase(changed,state,id,old,{1000000,1000000})==RepairResult::Filtered);
        assert(state.dist==original.dist && state.dist==dijkstra(changed,source).dist);
      }
    }
  }
}

static void reusable_bidirectional_workspace_differential() {
  for(std::uint64_t seed=100;seed<112;++seed){std::mt19937_64 rng(seed);Graph g(64);
    for(int i=0;i<640;++i){auto u=std::uint32_t(rng()%64),v=std::uint32_t(rng()%64);if(u!=v)g.add_edge(u,v,Weight(rng()%32));}
    for(int op=0;op<3000;++op){if(g.edge_count()&&op%11==0){auto id=std::uint32_t(rng()%g.edge_count());g.update_weight(id,Weight(rng()%32));continue;}
      auto s=std::uint32_t(rng()%64),t=std::uint32_t(rng()%64);assert(bidirectional_dijkstra_profiled(g,s,t).distance==dijkstra(g,s).dist[t]);}
  }
}

static void compact_resident_state_and_shared_repair_scratch() {
  Graph g(8);const auto cut=g.add_edge(0,1,1);for(std::uint32_t v=1;v<7;++v)g.add_edge(v,v+1,1);g.add_edge(0,7,40);
  auto state=dijkstra(g,0);assert(state.repair_mark.empty());assert(state.parent_edge[0]==kNoIndex);
  const auto expected=2u*sizeof(std::uint32_t)+g.vertex_count()*(sizeof(Distance)+4u*sizeof(std::uint32_t));
  assert(sssp_state_accounted_bytes(state)==expected);assert(sssp_state_accounted_bytes_for_vertices(g.vertex_count())==expected);
  assert(kPersistentStateAccountingVersion==2);
  const auto old=g.edge(cut);g.update_weight(cut,10);assert(repair_increase(g,state,cut,old,{100,100})==RepairResult::Repaired);
  assert(state.repair_mark.empty()&&state.dist==dijkstra(g,0).dist);
}

int main(){
  tight_nonparent_increase_is_read_only();parallel_tight_nonparent_increase_is_filtered();zero_weight_equal_witness_is_filtered();
  randomized_tight_nonparent_filter_differential();reusable_bidirectional_workspace_differential();compact_resident_state_and_shared_repair_scratch();
}
