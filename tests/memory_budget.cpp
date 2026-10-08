#include "ades/ades.hpp"
#include "ades/bounded_baselines.hpp"
#include "ades/memory_budget.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ades;

static void require(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}

static Graph make_graph(){
 Graph g(6);
 g.add_edge(0,1,2);g.add_edge(1,2,2);g.add_edge(2,3,2);
 g.add_edge(3,4,2);g.add_edge(4,5,2);g.add_edge(0,5,20);
 g.add_edge(1,4,3);g.add_edge(5,0,7);g.add_edge(2,0,4);
 return g;
}

static Distance exact(Graph g,std::uint32_t s,std::uint32_t t){
 return dijkstra(g,s).dist[t];
}

int main(){
 try{
  const auto g=make_graph();
  const auto probe=dijkstra(g,0);
  require(sssp_state_accounted_bytes(probe)==
              sssp_state_accounted_bytes_for_vertices(g.vertex_count()),
          "SSSP logical accounting formula mismatch");

  const auto bounded_one=bounded_resident_entry_accounted_bytes(probe);
  {
   BoundedResident b(g,ResidentMode::LocalRepair,99,bounded_one);
   require(b.query(0,5)==exact(g,0,5),"bounded first query not exact");
   require(b.resident_count()==1,"bounded budget should admit one state");
   require(b.accounted_algorithm_state_bytes()<=bounded_one,"bounded current bytes exceeded budget");
   require(b.peak_accounted_algorithm_state_bytes()<=bounded_one,"bounded peak bytes exceeded budget");
   require(b.query(1,5)==exact(g,1,5),"bounded churn query not exact");
   require(b.resident_count()==1,"bounded byte budget should retain one state");
   require(b.evictions()==1,"bounded byte budget should evict LRU state");
   require(b.accounted_algorithm_state_bytes()<=bounded_one,"bounded bytes exceeded after churn");
  }
  {
   BoundedResident b(g,ResidentMode::FullRebuild,99,bounded_one-1);
   require(b.query(0,5)==exact(g,0,5),"oversize bounded query not exact");
   require(b.resident_count()==0,"oversize state must not become persistent");
   require(b.accounted_algorithm_state_bytes()==0,"oversize bounded state was charged persistently");
  }

  const auto ades_one=ades_resident_entry_accounted_bytes(probe);
  const auto ades_budget=accounted_add(ades_one,ades_probation_entry_accounted_bytes());
  {
   Config cfg;
   cfg.resident_cap=99;
   cfg.probation_queries=1;
   cfg.promotion_ratio=0.0;
   cfg.persistent_state_budget_bytes=ades_budget;
   ADES a(g,cfg);
   require(a.query(0,5)==exact(g,0,5),"ADES first query not exact");
   require(a.resident_count()==1,"ADES should promote first source");
   require(a.accounted_algorithm_state_bytes()<=ades_budget,"ADES current bytes exceeded budget");
   require(a.query(1,5)==exact(g,1,5),"ADES byte-budget churn query not exact");
   require(a.resident_count()==1,"ADES byte budget should retain one resident state");
   require(a.stats().evictions>=1,"ADES byte pressure did not evict a resident");
   require(a.accounted_algorithm_state_bytes()<=ades_budget,"ADES bytes exceeded after replacement");
   require(a.peak_accounted_algorithm_state_bytes()<=ades_budget,"ADES peak accounted bytes exceeded budget");
   require(a.stats().persistent_state_budget_bytes==ades_budget,"ADES did not report configured budget");
  }

  // A budget too small for one probation record must not create hidden side
  // metadata. Exact cold queries continue to work.
  {
   Config cfg;
   cfg.resident_cap=99;
   cfg.probation_queries=10;
   cfg.promotion_ratio=0.0;
   cfg.persistent_state_budget_bytes=ades_probation_entry_accounted_bytes()-1;
   ADES a(g,cfg);
   require(a.query(0,5)==exact(g,0,5),"tiny-budget ADES query not exact");
   require(a.resident_count()==0,"tiny budget unexpectedly admitted state");
   require(a.accounted_algorithm_state_bytes()==0,"tiny budget accumulated policy metadata");
   require(a.stats().memory_budget_rejections>=1,"tiny budget rejection not reported");
  }

  std::cout<<"memory budget exact and enforced\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"memory budget test failure: "<<e.what()<<"\n";
  return 1;
 }
}
