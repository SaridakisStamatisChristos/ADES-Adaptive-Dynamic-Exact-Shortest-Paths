#include "ades/dynamic_repair.hpp"
#include "ades/shortest_paths.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>

using namespace ades;
using Clock=std::chrono::steady_clock;

static Graph make_graph(std::size_t n,const std::string& shape,std::uint64_t seed,std::uint32_t& changed){
 if(n<8)throw std::runtime_error("n must be >= 8");
 Graph g(n);std::mt19937_64 rng(seed);
 g.add_edge(0,1,1);
 changed=g.add_edge(0,2,2);
 g.add_edge(1,2,1);
 if(shape=="chain"){
  for(std::uint32_t v=2;v+1<n;++v)g.add_edge(v,v+1,1+Weight(rng()%4));
 }else if(shape=="binary"){
  for(std::uint32_t v=3;v<n;++v){
   const auto parent=std::uint32_t(2+(v-3)/2);
   g.add_edge(parent,v,1+Weight(rng()%4));
  }
 }else throw std::runtime_error("shape must be chain or binary");
 g.freeze_topology();
 return g;
}

int main(int argc,char**argv){
 if(argc!=5){std::cerr<<"usage: pr57_witness_profile <chain|binary> <vertices> <rounds> <seed>\n";return 2;}
 const std::string shape=argv[1];
 const auto n=std::size_t(std::stoull(argv[2]));
 const auto rounds=std::size_t(std::stoull(argv[3]));
 const auto seed=std::uint64_t(std::stoull(argv[4]));
 std::uint32_t changed=0;const auto base=make_graph(n,shape,seed,changed);
 const auto baseline_state=dijkstra(base,0);
 if(baseline_state.parent_edge[2]!=changed)throw std::runtime_error("benchmark did not select changed parent");
 std::uint64_t total_ns=0,total_discovered=0,checksum=0;
 for(std::size_t r=0;r<rounds;++r){
  auto g=base;auto state=baseline_state;const auto old=g.edge(changed);g.update_weight(changed,3);
  RepairWork work{};std::size_t discovered=0;
  const auto begin=Clock::now();
  const auto result=repair_increase(g,state,changed,old,{n+8,n+8,std::numeric_limits<std::uint64_t>::max()},&discovered,&work);
  total_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
  if(result!=RepairResult::Repaired)throw std::runtime_error("unexpected repair result");
  if(r==0||r+1==rounds){const auto oracle=dijkstra(g,0);if(state.dist!=oracle.dist)throw std::runtime_error("distance mismatch");}
  total_discovered+=discovered;
  checksum^=state.dist[n-1]+std::uint64_t(state.parent_edge[2])+std::uint64_t(r+1);
 }
 std::cout<<shape<<','<<n<<','<<rounds<<','<<seed<<','<<total_ns<<','
          <<(rounds?total_ns/rounds:0)<<','<<(rounds?total_discovered/rounds:0)<<','<<checksum<<'\n';
}
