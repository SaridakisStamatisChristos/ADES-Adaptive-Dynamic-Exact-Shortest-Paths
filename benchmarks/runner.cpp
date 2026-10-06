#include "ades/ades.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace ades;
struct Op{bool update;std::uint32_t a,b;Weight w;};
static std::vector<Op> trace(const Graph&g,std::uint64_t seed,std::size_t n){
 std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);
 for(std::size_t i=0;i<n;i++){if(g.edge_count()&&r()%4==0)x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});
 else x.push_back({false,(std::uint32_t)(r()%g.vertex_count()),(std::uint32_t)(r()%g.vertex_count()),0});}return x;
}
static std::uint64_t run_b0(Graph g,const std::vector<Op>&ops){
 auto t=std::chrono::steady_clock::now();for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else{volatile auto d=dijkstra(g,o.a).dist[o.b];(void)d;}
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
}
static std::uint64_t run_b1(Graph g,const std::vector<Op>&ops){
 auto t=std::chrono::steady_clock::now();for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else{volatile auto d=bidirectional_dijkstra(g,o.a,o.b);(void)d;}
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
}
static std::uint64_t run_b4(Graph g,const std::vector<Op>&ops){
 Config c;c.resident_cap=8;ADES a(std::move(g),c);auto t=std::chrono::steady_clock::now();
 for(auto&o:ops)if(o.update)a.update(o.a,o.w);else{volatile auto d=a.query(o.a,o.b);(void)d;}
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
}
int main(int argc,char**argv){
 if(argc<2){std::cerr<<"usage: ades_bench graph.gr.gz [ops] [seed]\n";return 2;}
 auto g=Graph::load_dimacs_gr_gz(argv[1]);std::size_t n=argc>2?std::strtoull(argv[2],nullptr,10):1000;std::uint64_t seed=argc>3?std::strtoull(argv[3],nullptr,10):7;
 auto ops=trace(g,seed,n);auto b0=run_b0(g,ops),b1=run_b1(g,ops),b4=run_b4(g,ops);
 std::cout<<"seed,ops,b0_ns,b1_ns,b4_ns\n"<<seed<<","<<n<<","<<b0<<","<<b1<<","<<b4<<"\n";
}
