#include "ades/ades.hpp"
#include "ades/repair_controller.hpp"
#include <cassert>
#include <iostream>
using namespace ades;
int main(){
 const std::uint32_t n=20000;Graph g(n);std::uint32_t cut=0;
 for(std::uint32_t v=1;v<n;v++){auto id=g.add_edge(v-1,v,1);if(v==1)cut=id;}
 Config c;c.resident_cap=1;c.probation_queries=1;c.promotion_ratio=0.0;c.repair_safety_ceiling={n,n*4};
 ADES a(std::move(g),c);assert(a.query(0,n-1)==n-1);a.update(cut,100);
 assert(a.query(0,n-1)==n+98);assert(a.stats().repair_aborts==1);
 std::cout<<"repair_aborts="<<a.stats().repair_aborts<<", rebuilds="<<a.stats().rebuilds<<", increase_repairs="<<a.stats().increase_repairs<<"\n";

 RepairController ctl(1.0,0.90);ctl.observe_rebuild(1000000);
 RepairWork small{};small.discovered_vertices=8;small.tree_edges=7;small.boundary_scans=8;small.restricted_scans=8;small.pq_pops=8;
 ctl.observe_repair(20000,small);auto before=ctl.budget();
 RepairWork costly{};costly.discovered_vertices=8;costly.tree_edges=7;costly.boundary_scans=5000;costly.restricted_scans=5000;costly.pq_pops=5000;
 ctl.observe_repair(800000,costly);auto after=ctl.budget();
 assert(after.vertices<before.vertices);
 std::cout<<"controller_vertices_before="<<before.vertices<<", after="<<after.vertices
          <<", work_per_vertex="<<ctl.work_per_vertex_estimate()<<"\n";
}
