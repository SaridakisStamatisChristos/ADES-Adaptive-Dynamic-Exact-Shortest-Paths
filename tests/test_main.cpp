#include "ades/ades.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace ades;
static void decrease_requires_propagation(){Graph g(3);auto e=g.add_edge(0,1,1);g.add_edge(1,2,1);ADES a(std::move(g),{1,1,1});assert(a.query(0,2)==2);a.update(e,0);assert(a.query(0,2)==1);}
static void tight_nonparent_increase(){Graph g(4);g.add_edge(0,1,1);g.add_edge(0,2,1);auto e=g.add_edge(1,3,1);g.add_edge(2,3,1);ADES a(std::move(g),{1,1,1});assert(a.query(0,3)==2);a.update(e,10);assert(a.query(0,3)==2);}
static void differential(){
 std::mt19937_64 rng(7); Graph base(30);for(int i=0;i<160;i++){auto u=rng()%30,v=rng()%30;if(u!=v)base.add_edge(u,v,rng()%21);}
 ADES a(base,{4,3,1}); for(int op=0;op<5000;op++){if(base.edge_count() && rng()%4==0){auto id=rng()%base.edge_count();auto w=rng()%21;base.update_weight(id,w);a.update(id,w);}
 else{auto s=rng()%30,t=rng()%30;auto ref=dijkstra(base,s).dist[t];auto got=a.query(s,t);if(ref!=got){std::cerr<<"mismatch "<<op<<"\n";std::abort();}}}
}
int main(){decrease_requires_propagation();tight_nonparent_increase();differential();std::cout<<"ADES tests passed\n";}
}