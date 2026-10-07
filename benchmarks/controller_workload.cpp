#include "ades/ades.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace ades;using Clock=std::chrono::steady_clock;
struct Step{std::uint32_t edge,target;Weight weight;};
static std::vector<std::uint32_t> subtree_sizes(const Graph&g,const SSSPState&s){
 std::vector<std::uint32_t> z(g.vertex_count(),1),order;order.reserve(g.vertex_count());
 std::vector<std::uint32_t> st{s.source};while(!st.empty()){auto u=st.back();st.pop_back();order.push_back(u);for(auto c=s.first_child[u];c>=0;c=s.next_sibling[(std::uint32_t)c])st.push_back((std::uint32_t)c);}
 for(auto it=order.rbegin();it!=order.rend();++it){auto v=*it;if(v!=s.source&&s.parent_edge[v]>=0)z[g.edge((std::uint32_t)s.parent_edge[v]).from]+=z[v];}return z;
}
static std::uint32_t pick_edge(const Graph&g,const SSSPState&s,const std::vector<std::uint32_t>&sz,bool catastrophic,std::mt19937_64&r){
 std::vector<std::uint32_t> ids;for(std::uint32_t v=0;v<g.vertex_count();v++)if(s.parent_edge[v]>=0){
  auto k=sz[v];bool ok=catastrophic?k>=std::max<std::size_t>(2,g.vertex_count()/4):k<=std::max<std::size_t>(8,g.vertex_count()/1000);
  if(ok)ids.push_back((std::uint32_t)s.parent_edge[v]);
 }
 if(ids.empty())for(std::uint32_t v=0;v<g.vertex_count();v++)if(s.parent_edge[v]>=0)ids.push_back((std::uint32_t)s.parent_edge[v]);
 if(ids.empty()){std::cerr<<"no reachable SPT edge\n";std::exit(4);}return ids[r()%ids.size()];
}
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: ades_controller_workload graph small|catastrophic [rounds] [seed] [policy]\n";return 2;}
 auto base=Graph::load_dimacs_gr_gz(argv[1]);std::string regime=argv[2],policy=argc>5?argv[5]:"work";std::size_t rounds=argc>3?std::strtoull(argv[3],nullptr,10):20;std::uint64_t seed=argc>4?std::strtoull(argv[4],nullptr,10):7;
 if(regime!="small"&&regime!="catastrophic"){std::cerr<<"unknown regime\n";return 2;}std::mt19937_64 rng(seed);
 std::uint32_t source=std::uint32_t(seed%base.vertex_count());auto model=dijkstra(base,source);
 for(std::size_t tries=0;tries<base.vertex_count();tries++){bool any=false;for(auto p:model.parent_edge)if(p>=0){any=true;break;}if(any)break;source=(source+1)%base.vertex_count();model=dijkstra(base,source);}
 std::vector<Step> steps;steps.reserve(rounds);
 for(std::size_t i=0;i<rounds;i++){auto sizes=subtree_sizes(base,model);auto id=pick_edge(base,model,sizes,regime=="catastrophic",rng);auto e=base.edge(id);Weight nw=e.weight+1+(rng()%17);base.update_weight(id,nw);model=dijkstra(base,source);std::vector<std::uint32_t> reachable;reachable.reserve(base.vertex_count());for(std::uint32_t v=0;v<base.vertex_count();v++)if(model.dist[v]<INF)reachable.push_back(v);
 if(reachable.empty()){std::cerr<<"resident source has no reachable target\n";return 4;}std::uint32_t target=reachable[rng()%reachable.size()];steps.push_back({id,target,nw});}
 auto graph=Graph::load_dimacs_gr_gz(argv[1]);Config cfg;cfg.resident_cap=1;cfg.probation_queries=1;cfg.promotion_ratio=0.0;cfg.repair_safety_ceiling={graph.vertex_count(),graph.edge_count()};
 if(policy=="fixed")cfg.repair_policy=RepairPolicy::Fixed;else if(policy=="vertex")cfg.repair_policy=RepairPolicy::VertexOnly;else if(policy=="work")cfg.repair_policy=RepairPolicy::WorkAware;else return 2;
 ADES a(graph,cfg);auto ref=dijkstra(graph,source);if(a.query(source,source)!=0||!a.resident(source)){std::cerr<<"failed to establish resident source\n";return 5;}
 auto start=Clock::now();for(auto&s:steps){graph.update_weight(s.edge,s.weight);a.update(s.edge,s.weight);auto expected=dijkstra(graph,source).dist[s.target],got=a.query(source,s.target);if(got!=expected){std::cerr<<"exactness failure\n";return 3;}}
 auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();auto st=a.stats();
 std::cout<<regime<<","<<policy<<","<<seed<<","<<rounds<<","<<source<<","<<ns<<","<<st.rebuilds<<","<<st.increase_repairs<<","<<st.repair_aborts<<","<<st.filtered_updates<<"\n";
}
