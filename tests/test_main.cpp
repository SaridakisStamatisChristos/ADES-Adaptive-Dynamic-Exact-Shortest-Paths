#include "ades/ades.hpp"
#include <cassert>
#include <iostream>
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
static void update_storm_does_not_promote_without_queries(){
 Graph g(10);std::vector<std::uint32_t> ids;for(std::uint32_t i=0;i<9;i++)ids.push_back(g.add_edge(i,i+1,1));
 Config c;c.resident_cap=2;c.probation_queries=1;c.promotion_ratio=0.0;ADES a(std::move(g),c);
 for(int i=0;i<100;i++)a.update(ids[i%ids.size()],1+(i&1));
 assert(a.resident_count()==0);assert(a.stats().promotions==0);
}
static void differential(){
 for(std::uint64_t seed=0;seed<12;seed++){std::mt19937_64 rng(seed);Graph base(35);for(int i=0;i<220;i++){auto u=rng()%35,v=rng()%35;if(u!=v)base.add_edge(u,v,rng()%21);}
  ADES a(base,resident_cfg());for(int op=0;op<12000;op++){if(base.edge_count()&&rng()%3==0){auto id=rng()%base.edge_count();auto w=rng()%21;base.update_weight(id,w);a.update(id,w);}
   else{auto s=rng()%35,t=rng()%35;auto ref=dijkstra(base,s).dist[t];auto got=a.query(s,t);if(ref!=got){std::cerr<<"mismatch seed="<<seed<<" op="<<op<<"\n";std::abort();}}
  }}
}
int main(){update_storm_does_not_promote_without_queries();rotating_semihot_sources_do_not_expand_cache();cost_aware_admission();decrease_requires_propagation();parent_increase_repairs_subtree();tight_nonparent_increase_cannot_be_ignored();early_abort_rebuild_is_exact();differential();std::cout<<"ADES repair tests passed\n";}
