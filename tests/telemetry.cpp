#include "ades/ades.hpp"
#include "ades/telemetry.hpp"

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

int main(){
 try{
  static_assert(kPublicationTelemetrySchemaVersion==2);
  Config cfg;
  cfg.resident_cap=2;
  cfg.probation_queries=1;
  cfg.promotion_ratio=0.0;
  ADES a(make_graph(),cfg);

  require(a.query(0,5)==7,"cold exactness failure");
  require(a.query(0,4)==5,"resident exactness failure");
  a.update(0,1);
  require(a.query(0,5)==6,"decrease exactness failure");
  a.update(0,5);
  require(a.query(0,5)==10,"increase exactness failure");

  const auto& st=a.stats();
  const auto& t=st.timing;
  require(st.cold_queries>=1,"cold path was not exercised");
  require(st.resident_queries>=1,"resident path was not exercised");
  require(st.promotions>=1,"promotion path was not exercised");
  require(t.reconciles(),"timing components do not reconcile");
  require(t.query_time_ns==t.query_components_ns(),"query timing conservation failed");
  require(t.update_time_ns==t.update_components_ns(),"update timing conservation failed");
  require(t.total_time_ns()==t.query_time_ns+t.update_time_ns,"total timing identity failed");
  require(t.cold_query_ns<=t.query_time_ns,"cold timing exceeds query domain");
  require(t.resident_query_ns<=t.query_time_ns,"resident timing exceeds query domain");
  require(t.promotion_ns<=t.query_time_ns,"promotion timing exceeds query domain");
  require(t.graph_update_ns<=t.update_time_ns,"graph mutation timing exceeds update domain");
  require(t.decrease_repair_ns+t.increase_repair_ns<=t.update_time_ns,
          "repair timing exceeds update domain");

  std::cout<<"telemetry schema v2 reconciles\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"telemetry test failure: "<<e.what()<<"\n";
  return 1;
 }
}
