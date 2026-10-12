#include "ades/dynamic_repair.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
namespace ades {
namespace {
struct RepairMembershipScratch {
 std::vector<std::uint32_t> mark;
 std::uint32_t epoch=0;
 std::uint32_t begin(std::size_t n){
  if(mark.size()<n)mark.resize(n,0);
  ++epoch;
  if(epoch==0){std::fill(mark.begin(),mark.end(),0);epoch=1;}
  return epoch;
 }
};
thread_local RepairMembershipScratch repair_membership_scratch;
}
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
 // A tight edge that is not the selected parent occurs nowhere in the selected
 // shortest-path tree. Every resident vertex therefore retains its selected
 // shortest-path witness after this edge increases. Distances cannot decrease
 // on an increase, so no semantic shortest-path state is invalidated.
 if(s.parent_edge[old.to]!=(std::int64_t)id)return RepairResult::Filtered;
 // Membership marks are reusable implementation scratch, not resident semantic
 // state. Keep repair_epoch as a diagnostic lifecycle counter while the actual
 // membership epoch lives in one thread-local workspace shared across repairs.
 if(s.repair_epoch==std::numeric_limits<std::uint32_t>::max())s.repair_epoch=1;else ++s.repair_epoch;
 auto&scratch=repair_membership_scratch;const auto epoch=scratch.begin(g.vertex_count());auto&mark=scratch.mark;
 std::vector<std::uint32_t> stack{old.to},nodes;std::size_t tree_work=0;
 while(!stack.empty()){
  auto u=stack.back();stack.pop_back();if(mark[u]==epoch)continue;mark[u]=epoch;nodes.push_back(u);
  if(discovered_vertices)*discovered_vertices=nodes.size();if(work)work->discovered_vertices=nodes.size();
  if(nodes.size()>budget.max_discovery_vertices|| (work&&work->total()>budget.max_total_work))return RepairResult::RebuildRequired;
  for(auto child=s.first_child[u];child>=0;child=s.next_sibling[(std::uint32_t)child]){
   ++tree_work;if(work)work->tree_edges++;
   if(tree_work>budget.max_discovery_tree_edges|| (work&&work->total()>budget.max_total_work))return RepairResult::RebuildRequired;
   stack.push_back((std::uint32_t)child);
  }
 }
 for(auto v:nodes){s.dist[v]=INF;set_parent(g,s,v,-1);}
 Q q;
 for(auto v:nodes){
  for(auto a:g.in(v)){
   if(work){work->boundary_scans++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}
   if(mark[a.to]!=epoch&&s.dist[a.to]<INF){
    auto nd=sat_add(s.dist[a.to],g.edge(a.edge_id).weight);
    if(nd<s.dist[v]){s.dist[v]=nd;set_parent(g,s,v,a.edge_id);}
   }
  }
 }
 for(auto v:nodes)if(s.dist[v]<INF)q.push({s.dist[v],v});
 while(!q.empty()){
  auto[du,u]=q.top();q.pop();if(work){work->pq_pops++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}if(du!=s.dist[u])continue;
  for(auto a:g.out(u)){
   if(work){work->restricted_scans++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}
   if(mark[a.to]==epoch){
    auto nd=sat_add(du,g.edge(a.edge_id).weight);
    if(nd<s.dist[a.to]){s.dist[a.to]=nd;set_parent(g,s,a.to,a.edge_id);q.push({nd,a.to});}
   }
  }
 }
 return RepairResult::Repaired;
}
}
