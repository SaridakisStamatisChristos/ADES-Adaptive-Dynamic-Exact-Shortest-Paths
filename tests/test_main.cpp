#include "ades/ades.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace ades;
static Config resident_cfg(){Config c;c.resident_cap=4;c.probation_queries=1;c.repair_budget={100000,500000};return c;}
static void decrease_requires_propagation(){Graph g(3);auto e=g.add_edge(0,1,1);g.add_edge(1,2,1);ADES a(std::move(g),resident_cfg());assert(a.query(0,2)==2);a.update(e,0);assert(a.query(0,2)==1);assert(a.stats().decrease_repairs==1);}
static void parent_increase_repairs_subtree(){Graph g(4);auto e=g.add_edge(0,1,1);g.add_edge(1,2,1);g.add_edge(2,3,1);g.add_edge(0,3,20);ADES a(std::move(g),resident_cfg());assert(a.query(0,3)==3);a.update(e,10);assert(a.query(0,3)==12);assert(a.stats().increase_repairs==1);}
static void tight_nonparent_increase_cannot_be_ignored(){Graph g(4);g.add_edge(0,1,1);g.add_edge(0,2,1);auto e=g.add_edge(1,3,1);g.add_edge(2,3,1);ADES a(std::move(g),resident_cfg());assert(a.query(0,3)==2);a.update(e,10);assert(a.query(0,3)==2);}
static void early_abort_rebuild_is_exact(){Graph g(6);auto e=g.add_edge(0,1,1);for(int i=1;i<5;i++)g.add_edge(i,i+1,1);Config c=resident_cfg();c.repair_budget={1,1};ADES a(std::move(g),c);assert(a.query(0,5)==5);a.update(e,9);assert(a.query(0,5)==13);assert(a.stats().repair_aborts==1);}
static void differential(){
 for(std::uint64_t seed=0;seed<12;seed++){std::mt19937_64 rng(seed);Graph base(35);for(int i=0;i<220;i++){auto u=rng()%35,v=rng()%35;if(u!=v)base.add_edge(u,v,rng()%21);}
  ADES a(base,resident_cfg());for(int op=0;op<12000;op++){if(base.edge_count()&&rng()%3==0){auto id=rng()%base.edge_count();auto w=rng()%21;base.update_weight(id,w);a.update(id,w);}
   else{auto s=rng()%35,t=rng()%35;auto ref=dijkstra(base,s).dist[t];auto got=a.query(s,t);if(ref!=got){std::cerr<<"mismatch seed="<<seed<<" op="<<op<<"\n";std::abort();}}
  }}
}
int main(){decrease_requires_propagation();parent_increase_repairs_subtree();tight_nonparent_increase_cannot_be_ignored();early_abort_rebuild_is_exact();differential();std::cout<<"ADES repair tests passed\n";}
