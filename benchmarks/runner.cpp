#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/coordinates.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace ades;using Clock=std::chrono::steady_clock;
struct Op{bool update;std::uint32_t a,b;Weight w;};
static std::vector<Op> mixed_trace(const Graph&g,std::uint64_t seed,std::size_t n){
 std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);for(std::size_t i=0;i<n;i++){
  if(g.edge_count()&&r()%4==0)x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});
  else x.push_back({false,(std::uint32_t)(r()%g.vertex_count()),(std::uint32_t)(r()%g.vertex_count()),0});}return x;
}
static std::vector<std::uint32_t> ordered_vertices(const std::vector<Coordinate>&c){
 std::vector<std::uint32_t> v(c.size());for(std::uint32_t i=0;i<v.size();i++)v[i]=i;
 std::sort(v.begin(),v.end(),[&](auto a,auto b){return c[a].x==c[b].x?c[a].y<c[b].y:c[a].x<c[b].x;});return v;
}
static std::vector<Op> spatial_trace(const Graph&g,const std::vector<Coordinate>&c,const std::string&kind,std::uint64_t seed,std::size_t n){
 if(c.size()!=g.vertex_count())throw std::runtime_error("coordinate/graph size mismatch");
 auto order=ordered_vertices(c);std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);const std::size_t N=order.size();
 const std::size_t band=std::max<std::size_t>(8,N/100),cluster=std::max<std::size_t>(16,N/20);
 for(std::size_t i=0;i<n;i++){
  if(g.edge_count()&&r()%5==0){x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});continue;}
  std::uint32_t s=0,t=0;
  if(kind=="local"){auto p=r()%N;s=order[p];auto lo=p>band?p-band:0,hi=std::min(N,p+band+1);t=order[lo+r()%(hi-lo)];}
  else if(kind=="cross"){auto q=std::max<std::size_t>(1,N/5);s=order[r()%q];t=order[N-q+r()%q];}
  else if(kind=="clustered"){auto center=(seed*2654435761ULL)%N,lo=center>cluster/2?center-cluster/2:0,hi=std::min(N,lo+cluster);s=order[lo+r()%(hi-lo)];t=order[lo+r()%(hi-lo)];}
  else if(kind=="moving"){auto p=(i*std::max<std::size_t>(1,N/std::max<std::size_t>(1,n)))%N;s=order[p];auto lo=p>band?p-band:0,hi=std::min(N,p+band+1);t=order[lo+r()%(hi-lo)];}
  else throw std::runtime_error("unknown workload");
  x.push_back({false,s,t,0});
 }return x;
}
static std::vector<Distance> oracle(Graph g,const std::vector<Op>&ops){std::vector<Distance> out;for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else out.push_back(dijkstra(g,o.a).dist[o.b]);return out;}
template<class E>static std::uint64_t run_engine(const char*name,E&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 std::size_t qi=0;auto t=Clock::now();for(auto&o:ops)if(o.update)e.update(o.a,o.w);else if(e.query(o.a,o.b)!=ref.at(qi++)){std::cerr<<name<<" exactness failure at query "<<qi-1<<"\n";std::exit(3);}
 return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();
}
struct FreshDijkstra{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return dijkstra(g,s).dist[t];}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
struct FreshBidir{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra(g,s,t);}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: ades_bench graph BASELINE [ops] [seed] [rep] [workload] [coords]\nworkload: mixed|local|cross|clustered|moving\n";return 2;}
 std::string baseline=argv[2],workload=argc>6?argv[6]:"mixed";auto base=Graph::load_dimacs_gr_gz(argv[1]);
 std::size_t n=argc>3?std::strtoull(argv[3],nullptr,10):1000;std::uint64_t seed=argc>4?std::strtoull(argv[4],nullptr,10):7;int rep=argc>5?std::atoi(argv[5]):0;
 std::vector<Op> ops;if(workload=="mixed")ops=mixed_trace(base,seed,n);else{if(argc<8){std::cerr<<"spatial workload requires coordinate file\n";return 2;}auto coords=load_dimacs_co_gz(argv[7],base.vertex_count());ops=spatial_trace(base,coords,workload,seed,n);}
 auto ref=oracle(base,ops);std::uint64_t ns=0;
 if(baseline=="B0"){FreshDijkstra e{base};ns=run_engine("B0",e,ops,ref);}else if(baseline=="B1"){FreshBidir e{base};ns=run_engine("B1",e,ops,ref);}
 else if(baseline=="B2"){AlwaysResident e(base,ResidentMode::FullRebuild);ns=run_engine("B2",e,ops,ref);}else if(baseline=="B3"){AlwaysResident e(base,ResidentMode::LocalRepair);ns=run_engine("B3",e,ops,ref);}
 else if(baseline=="B4"){Config cfg;cfg.resident_cap=8;ADES e(base,cfg);ns=run_engine("B4",e,ops,ref);}else{std::cerr<<"unknown baseline "<<baseline<<"\n";return 2;}
 std::cout<<baseline<<","<<rep<<","<<seed<<","<<workload<<","<<n<<","<<ref.size()<<","<<ns<<"\n";
}
