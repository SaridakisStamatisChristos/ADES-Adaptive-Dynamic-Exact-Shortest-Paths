#include "ades/ades.hpp"
#include <cassert>
#include <iostream>
using namespace ades;
int main(){
 const std::uint32_t n=20000;Graph g(n);std::uint32_t cut=0;
 for(std::uint32_t v=1;v<n;v++){auto id=g.add_edge(v-1,v,1);if(v==1)cut=id;}
 Config c;c.resident_cap=1;c.probation_queries=1;c.promotion_ratio=0.0;c.repair_safety_ceiling={n,n*4};
 ADES a(std::move(g),c);assert(a.query(0,n-1)==n-1);a.update(cut,100);
 assert(a.query(0,n-1)==n+98);
 std::cout<<"repair_aborts="<<a.stats().repair_aborts<<", rebuilds="<<a.stats().rebuilds<<", increase_repairs="<<a.stats().increase_repairs<<"\n";
}
