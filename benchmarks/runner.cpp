#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace ades;
using Clock=std::chrono::steady_clock;
struct Op{bool update;std::uint32_t a,b;Weight w;};
static std::vector<Op> trace(const Graph&g,std::uint64_t seed,std::size_t n){
 std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);
 for(std::size_t i=0;i<n;i++){if(g.edge_count()&&r()%4==0)x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});
 else x.push_back({false,(std::uint32_t)(r()%g.vertex_count()),(std::uint32_t)(r()%g.vertex_count()),0});}return x;
}
static std::vector<Distance> oracle(Graph g,const std::vector<Op>&ops){
 std::vector<Distance> out;for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else out.push_back(dijkstra(g,o.a).dist[o.b]);return out;
}
template<class Engine> static std::uint64_t run_engine(const char*name,Engine&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 std::size_t qi=0;auto t=Clock::now();
 for(auto&o:ops)if(o.update)e.update(o.a,o.w);else{auto d=e.query(o.a,o.b);if(d!=ref.at(qi++)){std::cerr<<name<<" exactness failure at query "<<qi-1<<"\n";std::exit(3);}}
 return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();
}
struct FreshDijkstra{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return dijkstra(g,s).dist[t];}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
struct FreshBidir{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra(g,s,t);}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: ades_bench graph.gr.gz BASELINE [ops] [seed] [rep]\nBASELINE: B0|B1|B2|B3|B4\n";return 2;}
 const std::string baseline=argv[2];auto base=Graph::load_dimacs_gr_gz(argv[1]);
 std::size_t n=argc>3?std::strtoull(argv[3],nullptr,10):1000;std::uint64_t seed=argc>4?std::strtoull(argv[4],nullptr,10):7;
 int rep=argc>5?std::atoi(argv[5]):0;auto ops=trace(base,seed,n);auto ref=oracle(base,ops);std::uint64_t ns=0;
 if(baseline=="B0"){FreshDijkstra e{base};ns=run_engine("B0",e,ops,ref);}
 else if(baseline=="B1"){FreshBidir e{base};ns=run_engine("B1",e,ops,ref);}
 else if(baseline=="B2"){AlwaysResident e(base,ResidentMode::FullRebuild);ns=run_engine("B2",e,ops,ref);}
 else if(baseline=="B3"){AlwaysResident e(base,ResidentMode::LocalRepair);ns=run_engine("B3",e,ops,ref);}
 else if(baseline=="B4"){Config cfg;cfg.resident_cap=8;ADES e(base,cfg);ns=run_engine("B4",e,ops,ref);}
 else{std::cerr<<"unknown baseline "<<baseline<<"\n";return 2;}
 std::cout<<baseline<<","<<rep<<","<<seed<<","<<n<<","<<ref.size()<<","<<ns<<"\n";
}
