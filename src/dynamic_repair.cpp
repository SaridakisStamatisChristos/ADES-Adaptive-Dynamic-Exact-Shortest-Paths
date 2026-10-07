#include "ades/dynamic_repair.hpp"
#include <functional>
#include <queue>
namespace ades {
using Q=std::priority_queue<std::pair<Distance,std::uint32_t>,std::vector<std::pair<Distance,std::uint32_t>>,std::greater<std::pair<Distance,std::uint32_t>>>;
RepairResult repair_decrease(const Graph& g,SSSPState& s,std::uint32_t id){
 const auto&e=g.edge(id);if(s.dist[e.from]>=INF)return RepairResult::Filtered;
 auto cand=sat_add(s.dist[e.from],e.weight);if(cand>=s.dist[e.to])return RepairResult::Filtered;
 Q q;s.dist[e.to]=cand;set_parent(g,s,e.to,id);q.push({cand,e.to});
 while(!q.empty()){auto[du,u]=q.top();q.pop();if(du!=s.dist[u])continue;
  for(auto a:g.out(u)){auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<s.dist[a.to]){s.dist[a.to]=nd;set_parent(g,s,a.to,a.edge_id);q.push({nd,a.to});}}
 }return RepairResult::Repaired;
}
RepairResult repair_increase(const Graph& g,SSSPState& s,std::uint32_t id,const Edge& old,RepairBudget budget,std::size_t* discovered_vertices,RepairWork* work){
 if(discovered_vertices)*discovered_vertices=0;if(work)*work={};
 if(s.dist[old.from]>=INF||sat_add(s.dist[old.from],old.weight)!=s.dist[old.to])return RepairResult::Filtered;
 if(s.parent_edge[old.to]!=(std::int64_t)id)return RepairResult::RebuildRequired;
 const auto n=g.vertex_count();
 std::vector<unsigned char> affected(n,0);std::vector<std::uint32_t> stack{old.to},nodes;std::size_t tree_work=0;
 while(!stack.empty()){
  auto u=stack.back();stack.pop_back();if(affected[u])continue;affected[u]=1;nodes.push_back(u);
  if(discovered_vertices)*discovered_vertices=nodes.size();if(work)work->discovered_vertices=nodes.size();
  if(nodes.size()>budget.max_discovery_vertices)return RepairResult::RebuildRequired;
  for(auto child=s.first_child[u];child>=0;child=s.next_sibling[(std::uint32_t)child]){
   ++tree_work;if(work)work->tree_edges++;
   if(tree_work>budget.max_discovery_tree_edges)return RepairResult::RebuildRequired;
   stack.push_back((std::uint32_t)child);
  }
 }
 for(auto v:nodes){s.dist[v]=INF;set_parent(g,s,v,-1);}
 Q q;
 for(auto v:nodes){
  for(auto a:g.in(v)){
   if(work)work->boundary_scans++;
   if(!affected[a.to]&&s.dist[a.to]<INF){
    auto nd=sat_add(s.dist[a.to],g.edge(a.edge_id).weight);
    if(nd<s.dist[v]){s.dist[v]=nd;set_parent(g,s,v,a.edge_id);}
   }
  }
 }
 for(auto v:nodes)if(s.dist[v]<INF)q.push({s.dist[v],v});
 while(!q.empty()){
  auto[du,u]=q.top();q.pop();if(work)work->pq_pops++;if(du!=s.dist[u])continue;
  for(auto a:g.out(u)){
   if(work)work->restricted_scans++;
   if(affected[a.to]){
    auto nd=sat_add(du,g.edge(a.edge_id).weight);
    if(nd<s.dist[a.to]){s.dist[a.to]=nd;set_parent(g,s,a.to,a.edge_id);q.push({nd,a.to});}
   }
  }
 }
 return RepairResult::Repaired;
}
}
