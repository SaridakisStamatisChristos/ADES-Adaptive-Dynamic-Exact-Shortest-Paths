#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/bounded_baselines.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
using namespace ades;
static Config resident_cfg(){Config c;c.resident_cap=4;c.probation_queries=1;c.promotion_ratio=0.0;c.repair_safety_ceiling={100000,500000};return c;}
static void decrease_requires_propagation(){Graph g(3);auto e=g.add_edge(0,1,1);g.add_edge(1,2,1);ADES a(std::move(g),resident_cfg());assert(a.query(0,2)==2);a.update(e,0);assert(a.query(0,2)==1);assert(a.stats().decrease_repairs==1);}
static void parent_increase_repairs_subtree(){Graph g(4);auto e=g.add_edge(0,1,1);g.add_edge(1,2,1);g.add_edge(2,3,1);g.add_edge(0,3,20);ADES a(std::move(g),resident_cfg());assert(a.query(0,3)==3);a.update(e,10);assert(a.query(0,3)==12);assert(a.stats().increase_repairs==1);}
static void tight_nonparent_increase_cannot_be_ignored(){Graph g(4);g.add_edge(0,1,1);g.add_edge(0,2,1);auto e=g.add_edge(1,3,1);g.add_edge(2,3,1);ADES a(std::move(g),resident_cfg());assert(a.query(0,3)==2);a.update(e,10);assert(a.query(0,3)==2);}
static void early_abort_rebuild_is_exact(){Graph g(6);auto e=g.add_edge(0,1,1);for(int i=1;i<5;i++)g.add_edge(i,i+1,1);Config c=resident_cfg();c.repair_safety_ceiling={1,1};ADES a(std::move(g),c);assert(a.query(0,5)==5);a.update(e,9);assert(a.query(0,5)==13);assert(a.stats().repair_aborts==1);}
static void cost_aware_admission(){
 Graph local(101);for(std::uint32_t i=0;i<100;i++)local.add_edge(i,i+1,1);
 Config lc;lc.resident_cap=2;lc.probation_queries=2;lc.promotion_ratio=1.0;ADES a(std::move(local),lc);
 for(int i=0;i<20;i++)assert(a.query(0,1)==1);assert(!a.resident(0));
 Graph broad(101);for(std::uint32_t i=0;i<100;i++)broad.add_edge(i,i+1,1);
 ADES b(std::move(broad),lc);for(int i=0;i<3&&!b.resident(0);i++)assert(b.query(0,100)==100);assert(b.resident(0));
}
static void rotating_semihot_sources_do_not_expand_cache(){
 Graph g(20);for(std::uint32_t i=0;i<19;i++)g.add_edge(i,i+1,1);
 Config c;c.resident_cap=2;c.probation_queries=1;c.promotion_ratio=0.0;c.cooldown_queries=50;ADES a(std::move(g),c);
 for(int round=0;round<8;round++)for(std::uint32_t s=0;s<6;s++){a.query(s,19);assert(a.resident_count()<=2);}
 assert(a.stats().evictions>0);assert(a.stats().cooldown_blocks>0);
}
static void weak_candidate_cannot_evict_hot_resident(){
 Graph g(30);for(std::uint32_t i=0;i<29;i++)g.add_edge(i,i+1,1);
 Config c;c.resident_cap=2;c.probation_queries=1;c.promotion_ratio=0.0;c.admission_hysteresis=1.10;c.cooldown_queries=20;
 ADES a(std::move(g),c);
 assert(a.query(0,29)==29);assert(a.query(1,29)==28);assert(a.resident(0)&&a.resident(1));
 for(int i=0;i<12;i++){assert(a.query(0,29)==29);assert(a.query(1,29)==28);}
 auto evictions=a.stats().evictions;
 for(std::uint32_t s=2;s<12;s++)a.query(s,29);
 assert(a.resident(0)&&a.resident(1));assert(a.stats().evictions==evictions);assert(a.stats().admission_rejections>=10);
}
static void update_storm_does_not_promote_without_queries(){
 Graph g(10);std::vector<std::uint32_t> ids;for(std::uint32_t i=0;i<9;i++)ids.push_back(g.add_edge(i,i+1,1));
 Config c;c.resident_cap=2;c.probation_queries=1;c.promotion_ratio=0.0;ADES a(std::move(g),c);
 for(int i=0;i<100;i++)a.update(ids[i%ids.size()],1+(i&1));
 assert(a.resident_count()==0);assert(a.stats().promotions==0);
}
static void abort_discovery_is_read_only(){
 Graph g(7);auto e=g.add_edge(0,1,1);for(std::uint32_t i=1;i<6;i++)g.add_edge(i,i+1,1);
 auto s=dijkstra(g,0);
 auto dist=s.dist;auto parent=s.parent_edge;auto first=s.first_child;auto next=s.next_sibling;auto prev=s.prev_sibling;
 auto old=g.edge(e);g.update_weight(e,20);
 std::size_t discovered=0;auto r=repair_increase(g,s,e,old,{1,1},&discovered);
 assert(r==RepairResult::RebuildRequired);assert(discovered>1);
 assert(s.dist==dist);assert(s.parent_edge==parent);assert(s.first_child==first);assert(s.next_sibling==next);assert(s.prev_sibling==prev);
}
static void repeated_reparent_preserves_spt_links(){
 Graph g(5);auto a=g.add_edge(0,1,5);auto b=g.add_edge(0,2,1);auto c=g.add_edge(2,1,1);g.add_edge(1,3,1);g.add_edge(3,4,1);
 auto s=dijkstra(g,0);assert(s.parent_edge[1]==(std::int64_t)c);
 auto oldc=g.edge(c);g.update_weight(c,10);assert(repair_increase(g,s,c,oldc,{100,100})==RepairResult::Repaired);assert(s.parent_edge[1]==(std::int64_t)a);
 auto olda=g.edge(a);g.update_weight(a,20);assert(repair_increase(g,s,a,olda,{100,100})==RepairResult::Repaired);
 auto ref=dijkstra(g,0);assert(s.dist==ref.dist);assert(s.dist[4]==13);(void)b;
}
static void repair_work_accounting_is_complete(){
 Graph g(6);auto cut=g.add_edge(0,1,1);g.add_edge(1,2,1);g.add_edge(2,3,1);g.add_edge(3,4,1);g.add_edge(4,5,1);g.add_edge(0,5,20);
 auto s=dijkstra(g,0);auto old=g.edge(cut);g.update_weight(cut,10);RepairWork w{};std::size_t discovered=0;
 auto r=repair_increase(g,s,cut,old,{100,100},&discovered,&w);
 assert(r==RepairResult::Repaired);assert(w.discovered_vertices==discovered);assert(w.discovered_vertices==5);
 assert(w.tree_edges==4);assert(w.boundary_scans>0);assert(w.restricted_scans>0);assert(w.pq_pops>0);assert(w.total()>=w.discovered_vertices);
 assert(s.dist[5]==14);
}
static void work_aware_controller_tightens_after_expensive_repairs(){
 RepairController c(1.0,0.90);c.observe_rebuild(1000000);
 RepairWork cheap{};cheap.discovered_vertices=10;cheap.tree_edges=9;cheap.boundary_scans=10;cheap.restricted_scans=10;cheap.pq_pops=10;
 c.observe_repair(10000,cheap);auto generous=c.budget();
 RepairWork expensive=cheap;expensive.boundary_scans=10000;expensive.restricted_scans=10000;expensive.pq_pops=10000;
 c.observe_repair(900000,expensive);auto tight=c.budget();
 assert(tight.vertices<generous.vertices);assert(tight.tree_edges<=generous.tree_edges);
}
static void shortest_path_corner_cases(){
 Graph g(7);
 g.add_edge(0,1,0);g.add_edge(1,2,0);g.add_edge(0,2,0);
 g.add_edge(2,3,5);g.add_edge(0,3,5);
 auto parallel_slow=g.add_edge(3,4,9);auto parallel_fast=g.add_edge(3,4,1);(void)parallel_slow;(void)parallel_fast;
 auto s=dijkstra(g,0);
 assert(s.dist[0]==0);assert(s.dist[1]==0);assert(s.dist[2]==0);assert(s.dist[3]==5);assert(s.dist[4]==6);
 assert(s.dist[5]==INF);assert(s.dist[6]==INF);
 assert(bidirectional_dijkstra(g,0,0)==0);assert(bidirectional_dijkstra(g,0,4)==6);assert(bidirectional_dijkstra(g,4,0)==INF);
}
static void saturating_distance_arithmetic(){
 assert(sat_add(INF,1)==INF);assert(sat_add(INF-1,1)==INF);assert(sat_add(INF-2,1)==INF-1);
 Graph g(3);g.add_edge(0,1,INF-2);g.add_edge(1,2,100);g.add_edge(0,2,INF-1);
 auto s=dijkstra(g,0);assert(s.dist[2]==INF-1);assert(bidirectional_dijkstra(g,0,2)==INF-1);
}
static void equal_distance_parent_cycle_prevention(){
 Graph g(4);
 g.add_edge(0,1,0);g.add_edge(0,2,0);g.add_edge(1,2,0);g.add_edge(2,1,0);g.add_edge(1,3,1);g.add_edge(2,3,1);
 auto s=dijkstra(g,0);assert(s.dist[1]==0&&s.dist[2]==0&&s.dist[3]==1);
 for(std::uint32_t v=1;v<4;v++){
  std::uint32_t cur=v;std::size_t steps=0;
  while(cur!=0){assert(s.parent_edge[cur]>=0);cur=g.edge((std::uint32_t)s.parent_edge[cur]).from;assert(++steps<=g.vertex_count());}
 }
}
static void dynamic_parallel_and_zero_weight_updates(){
 Graph g(4);auto slow=g.add_edge(0,1,7);auto fast=g.add_edge(0,1,2);auto tail=g.add_edge(1,2,0);g.add_edge(2,3,1);
 ADES a(std::move(g),resident_cfg());assert(a.query(0,3)==3);
 a.update(fast,9);assert(a.query(0,3)==8);
 a.update(slow,1);assert(a.query(0,3)==2);
 a.update(tail,5);assert(a.query(0,3)==7);
}
static void epoch_membership_survives_repeated_repairs_and_wrap(){
 Graph g(6);auto cut=g.add_edge(0,1,1);g.add_edge(1,2,1);g.add_edge(2,3,1);g.add_edge(3,4,1);g.add_edge(4,5,1);g.add_edge(0,5,50);
 auto s=dijkstra(g,0);
 for(Weight w: {Weight(2),Weight(3),Weight(4)}){
  auto old=g.edge(cut);g.update_weight(cut,w);RepairWork work{};
  assert(repair_increase(g,s,cut,old,{100,100},nullptr,&work)==RepairResult::Repaired);
  assert(s.dist==dijkstra(g,0).dist);assert(work.discovered_vertices==5);
 }
 s.repair_epoch=std::numeric_limits<std::uint32_t>::max();
 auto old=g.edge(cut);g.update_weight(cut,5);
 assert(repair_increase(g,s,cut,old,{100,100})==RepairResult::Repaired);
 assert(s.repair_epoch==1);assert(s.dist==dijkstra(g,0).dist);
}
static void repair_policy_modes_remain_exact(){
 for(auto policy:{RepairPolicy::Fixed,RepairPolicy::VertexOnly,RepairPolicy::WorkAware}){
  Graph g(8);auto cut=g.add_edge(0,1,1);for(std::uint32_t i=1;i<7;i++)g.add_edge(i,i+1,1);g.add_edge(0,7,50);
  Config c=resident_cfg();c.repair_policy=policy;ADES a(std::move(g),c);assert(a.query(0,7)==7);
  a.update(cut,10);assert(a.query(0,7)==16);
 }
}
static void differential(){
 for(std::uint64_t seed=0;seed<12;seed++){
  std::mt19937_64 rng(seed);Graph base(35);
  for(int i=0;i<220;i++){auto u=rng()%35,v=rng()%35;if(u!=v)base.add_edge(u,v,rng()%21);}
  ADES subject(base,resident_cfg());
  for(int op=0;op<12000;op++){
   if(base.edge_count()&&rng()%3==0){auto id=rng()%base.edge_count();auto w=rng()%21;base.update_weight(id,w);subject.update(id,w);}
   else{
    auto s=rng()%35,t=rng()%35;auto expected=dijkstra(base,s).dist[t],got=subject.query(s,t);
    if(expected!=got){
     std::ofstream out("ades_failure_seed_"+std::to_string(seed)+".txt");
     out<<"# deterministic differential failure\nseed "<<seed<<"\nop "<<op<<"\nsource "<<s<<"\ntarget "<<t<<"\nexpected "<<expected<<"\ngot "<<got<<"\n";
     std::cerr<<"mismatch seed="<<seed<<" op="<<op<<" artifact=ades_failure_seed_"<<seed<<".txt\n";std::abort();
    }
   }
  }
 }
}
static void unbounded_baseline_residency_accounting(){
 Graph g(4);g.add_edge(0,1,1);g.add_edge(1,2,1);g.add_edge(2,3,1);
 for(auto mode:{ResidentMode::FullRebuild,ResidentMode::LocalRepair}){
  AlwaysResident b(g,mode);
  assert(b.resident_count()==0);
  assert(b.query(0,3)==3);assert(b.resident_count()==1);
  assert(b.query(0,2)==2);assert(b.resident_count()==1);
  assert(b.query(1,3)==2);assert(b.resident_count()==2);
  b.update(0,2);assert(b.resident_count()==2);
  assert(b.query(0,3)==4);assert(b.query(1,3)==2);
 }
}
static void bounded_comparator_exactness_and_lru(){
 Graph g(6);auto edge=g.add_edge(0,1,1);
 for(std::uint32_t i=1;i<5;i++)g.add_edge(i,i+1,1);
 for(auto mode:{ResidentMode::FullRebuild,ResidentMode::LocalRepair}){
  BoundedResident b(g,mode,2);
  assert(b.query(0,5)==5);assert(b.query(1,5)==4);
  assert(b.resident_count()==2);assert(b.misses()==2);
  assert(b.query(0,5)==5);assert(b.hits()==1);
  assert(b.query(2,5)==3);assert(b.evictions()==1);
  assert(b.query(1,5)==4);assert(b.evictions()==2);
  assert(b.resident_count()==2);
  b.update(edge,10);
  assert(b.query(0,5)==14);assert(b.query(1,5)==4);
  assert(b.resident_count()<=2);
  b.update(edge,0);
  assert(b.query(0,5)==4);
 }
 bool rejected=false;
 try{BoundedResident bad(g,ResidentMode::FullRebuild,0);}
 catch(const std::invalid_argument&){rejected=true;}
 assert(rejected);
}
static void bounded_comparator_differential(){
 for(auto mode:{ResidentMode::FullRebuild,ResidentMode::LocalRepair}){
  std::mt19937_64 rng(19);Graph g(20);
  for(int i=0;i<100;i++){auto u=rng()%20,v=rng()%20;if(u!=v)g.add_edge(u,v,rng()%15);}
  BoundedResident b(g,mode,3);
  for(int i=0;i<600;i++){
   if(i%5==0){auto id=rng()%g.edge_count();auto w=rng()%15;g.update_weight(id,w);b.update(id,w);}
   else {auto s=rng()%20,t=rng()%20;assert(b.query(s,t)==dijkstra(g,s).dist[t]);}
   assert(b.resident_count()<=3);
  }
 }
}
int main(){bounded_comparator_exactness_and_lru();bounded_comparator_differential();unbounded_baseline_residency_accounting();shortest_path_corner_cases();saturating_distance_arithmetic();equal_distance_parent_cycle_prevention();dynamic_parallel_and_zero_weight_updates();epoch_membership_survives_repeated_repairs_and_wrap();repair_policy_modes_remain_exact();repair_work_accounting_is_complete();work_aware_controller_tightens_after_expensive_repairs();weak_candidate_cannot_evict_hot_resident();abort_discovery_is_read_only();repeated_reparent_preserves_spt_links();update_storm_does_not_promote_without_queries();rotating_semihot_sources_do_not_expand_cache();cost_aware_admission();decrease_requires_propagation();parent_increase_repairs_subtree();tight_nonparent_increase_cannot_be_ignored();early_abort_rebuild_is_exact();differential();std::cout<<"ADES repair tests passed\n";}
