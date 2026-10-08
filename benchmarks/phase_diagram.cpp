#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace ades; using Clock=std::chrono::steady_clock;
struct Op{bool update;std::uint32_t a,b;Weight w;};
static std::vector<std::uint32_t> hot_pool(std::size_t n,std::size_t k,std::mt19937_64&r){
 std::vector<std::uint32_t> v(n);for(std::uint32_t i=0;i<n;i++)v[i]=i;std::shuffle(v.begin(),v.end(),r);v.resize(std::min(k,n));return v;
}
static std::uint32_t pick_source(const std::string&family,const std::vector<std::uint32_t>&hot,std::size_t q,std::size_t epoch,std::size_t n,std::mt19937_64&r){
 if(family=="uniform")return r()%n;
 if(family=="single-hot")return (r()%100<95)?hot[0]:r()%n;
 if(family=="rotating-hot")return (r()%100<90)?hot[(q/epoch)%hot.size()]:r()%n;
 if(family=="hot-pool")return (r()%100<90)?hot[r()%hot.size()]:r()%n;
 if(family=="churn")return hot[(q/epoch)%hot.size()];
 if(family=="zipf"){double u=std::generate_canonical<double,53>(r),h=0;for(std::size_t i=1;i<=hot.size();++i)h+=1.0/double(i);double c=0;for(std::size_t i=1;i<=hot.size();++i){c+=(1.0/double(i))/h;if(u<=c)return hot[i-1];}return hot.back();}
 throw std::runtime_error("unknown source family");
}
static std::vector<Op> make_trace(const Graph&g,const std::string&family,std::uint64_t seed,std::size_t queries,std::size_t update_every,std::size_t hot_sources,std::size_t epoch){
 Graph trace_g=g;std::mt19937_64 r(seed);auto hot=hot_pool(trace_g.vertex_count(),std::max<std::size_t>(1,hot_sources),r);std::vector<Op> ops;ops.reserve(queries+queries/std::max<std::size_t>(1,update_every));
 for(std::size_t q=0;q<queries;q++){if(update_every&&q&&q%update_every==0){auto id=std::uint32_t(r()%trace_g.edge_count());auto old=trace_g.edge(id).weight;Weight nw=(r()&1)?old+1+(r()%31):(old?old-std::min<Weight>(old,r()%std::min<Weight>(old+1,31)):0);trace_g.update_weight(id,nw);ops.push_back({true,id,0,nw});}
  auto s=pick_source(family,hot,q,std::max<std::size_t>(1,epoch),g.vertex_count(),r);auto t=std::uint32_t(r()%g.vertex_count());ops.push_back({false,s,t,0});}
 return ops;
}
static std::vector<Distance> oracle(Graph g,const std::vector<Op>&ops){std::vector<Distance>x;for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else x.push_back(dijkstra(g,o.a).dist[o.b]);return x;}
template<class E>static std::uint64_t run(E&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){std::size_t qi=0;auto s=Clock::now();for(auto&o:ops){if(o.update)e.update(o.a,o.w);else if(e.query(o.a,o.b)!=ref[qi++]){std::cerr<<"exactness failure\n";std::exit(3);}}return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-s).count();}
struct B1{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra(g,s,t);}void update(std::uint32_t e,Weight w){g.update_weight(e,w);}};
int main(int argc,char**argv){
 if(argc<9){std::cerr<<"usage: ades_phase graph B1|B2|B3|B4|ALL family queries update_every hot_sources epoch seed [cap]\\n";return 2;}
 auto g=Graph::load_dimacs_gr_gz(argv[1]);std::string base=argv[2],family=argv[3];auto nq=std::strtoull(argv[4],0,10),ue=std::strtoull(argv[5],0,10),hs=std::strtoull(argv[6],0,10),ep=std::strtoull(argv[7],0,10);std::uint64_t seed=std::strtoull(argv[8],0,10);std::size_t cap=argc>9?std::strtoull(argv[9],0,10):8;
 auto ops=make_trace(g,family,seed,nq,ue,hs,ep);auto ref=oracle(g,ops);
 std::size_t updates=0;for(auto&o:ops)updates+=o.update;
 auto emit=[&](const std::string& b,std::uint64_t ns,const Stats& st){
  std::cout<<b<<","<<family<<","<<seed<<","<<nq<<","<<updates<<","<<ue<<","<<hs<<","<<ep<<","<<cap<<","<<ns<<","<<st.cold_queries<<","<<st.resident_queries<<","<<st.promotions<<","<<st.evictions<<","<<st.rebuilds<<","<<st.repair_aborts<<"\n";
 };
 auto one=[&](const std::string& b){
  std::uint64_t ns=0;Stats st{};
  if(b=="B1"){B1 e{g};ns=run(e,ops,ref);}
  else if(b=="B2"){AlwaysResident e(g,ResidentMode::FullRebuild);ns=run(e,ops,ref);}
  else if(b=="B3"){AlwaysResident e(g,ResidentMode::LocalRepair);ns=run(e,ops,ref);}
  else if(b=="B4"){Config cfg;cfg.resident_cap=cap;ADES e(g,cfg);ns=run(e,ops,ref);st=e.stats();}
  else return false;
  emit(b,ns,st);return true;
 };
 if(base=="ALL"){for(const char* b:{"B1","B2","B3","B4"})if(!one(b))return 2;}
 else if(!one(base))return 2;
}
