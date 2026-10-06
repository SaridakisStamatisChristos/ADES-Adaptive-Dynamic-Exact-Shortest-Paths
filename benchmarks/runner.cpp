#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <sys/resource.h>
#include <vector>
using namespace ades;
using Clock=std::chrono::steady_clock;
struct Op{bool update;std::uint32_t a,b;Weight w;};
struct Result{std::string name;std::uint64_t ns;long rss_kb;};
static std::vector<Op> trace(const Graph&g,std::uint64_t seed,std::size_t n){
 std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);
 for(std::size_t i=0;i<n;i++){if(g.edge_count()&&r()%4==0)x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});
 else x.push_back({false,(std::uint32_t)(r()%g.vertex_count()),(std::uint32_t)(r()%g.vertex_count()),0});}return x;
}
static std::vector<Distance> oracle(Graph g,const std::vector<Op>&ops){
 std::vector<Distance> out;for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else out.push_back(dijkstra(g,o.a).dist[o.b]);return out;
}
template<class Engine> static Result run_engine(const char*name,Engine&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 std::size_t qi=0;auto t=Clock::now();
 for(auto&o:ops)if(o.update)e.update(o.a,o.w);else{auto d=e.query(o.a,o.b);if(d!=ref.at(qi++)){std::cerr<<name<<" exactness failure at query "<<qi-1<<"\n";std::exit(3);}}
 auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();rusage ru{};getrusage(RUSAGE_SELF,&ru);return{name,(std::uint64_t)ns,ru.ru_maxrss};
}
struct FreshDijkstra{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return dijkstra(g,s).dist[t];}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
struct FreshBidir{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra(g,s,t);}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
int main(int argc,char**argv){
 if(argc<2){std::cerr<<"usage: ades_bench graph.gr.gz [ops] [seed] [reps]\n";return 2;}
 auto base=Graph::load_dimacs_gr_gz(argv[1]);std::size_t n=argc>2?std::strtoull(argv[2],nullptr,10):1000;
 std::uint64_t seed=argc>3?std::strtoull(argv[3],nullptr,10):7;int reps=argc>4?std::atoi(argv[4]):3;
 auto ops=trace(base,seed,n);auto ref=oracle(base,ops);
 std::cout<<"baseline,rep,seed,ops,queries,ns,maxrss_kb\n";
 for(int rep=0;rep<reps;rep++){
  FreshDijkstra b0{base};FreshBidir b1{base};AlwaysResident b2(base,ResidentMode::FullRebuild),b3(base,ResidentMode::LocalRepair);
  Config cfg;cfg.resident_cap=8;ADES b4(base,cfg);
  for(auto r:{run_engine("B0",b0,ops,ref),run_engine("B1",b1,ops,ref),run_engine("B2",b2,ops,ref),run_engine("B3",b3,ops,ref),run_engine("B4",b4,ops,ref)})
   std::cout<<r.name<<","<<rep<<","<<seed<<","<<n<<","<<ref.size()<<","<<r.ns<<","<<r.rss_kb<<"\n";
 }
}
