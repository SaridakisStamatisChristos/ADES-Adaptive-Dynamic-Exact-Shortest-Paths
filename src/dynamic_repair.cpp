#include "ades/dynamic_repair.hpp"
#include "ades/radix_queue.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace ades {
namespace {
constexpr std::size_t kWitnessDiscoveryTrigger=256;
constexpr std::size_t kWitnessProbeLimit=64;
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
RepairResult repair_decrease(const Graph& g,SSSPState& s,std::uint32_t id){
 const auto&e=g.edge(id);if(s.dist[e.from]>=INF)return RepairResult::Filtered;
 auto cand=sat_add(s.dist[e.from],e.weight);if(cand>=s.dist[e.to])return RepairResult::Filtered;
 RadixQueue q;s.dist[e.to]=cand;set_parent(g,s,e.to,id);q.push(cand,e.to);
 while(!q.empty()){auto[du,u]=q.pop();if(du!=s.dist[u])continue;
  for(auto a:g.out(u)){auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<s.dist[a.to]){s.dist[a.to]=nd;set_parent(g,s,a.to,a.edge_id);q.push(nd,a.to);}}
 }return RepairResult::Repaired;
}
RepairResult repair_increase(const Graph& g,SSSPState& s,std::uint32_t id,const Edge& old,RepairBudget budget,std::size_t* discovered_vertices,RepairWork* work){
 if(discovered_vertices)*discovered_vertices=0;
 if(work)*work={};
 if(s.dist[old.from]>=INF||sat_add(s.dist[old.from],old.weight)!=s.dist[old.to])return RepairResult::Filtered;
 // A tight edge that is not the selected parent occurs nowhere in the selected
 // shortest-path tree. Every resident vertex therefore retains its selected
 // shortest-path witness after this edge increases. Distances cannot decrease
 // on an increase, so no semantic shortest-path state is invalidated.
 if(s.parent_edge[old.to]!=id)return RepairResult::Filtered;

 // PR57 v2 deliberately does NOT probe for alternate witnesses yet. The broad
 // v1 matrix showed that paying an incoming-edge probe on every selected-parent
 // increase can regress workloads dominated by small/cheap repairs. Discovery
 // below is already required by PR55, so witness probing is activated only after
 // enough affected vertices have been observed to amortize its bounded cost.
 if(s.repair_epoch==std::numeric_limits<std::uint32_t>::max())s.repair_epoch=1;else ++s.repair_epoch;
 auto&scratch=repair_membership_scratch;const auto epoch=scratch.begin(g.vertex_count());auto&mark=scratch.mark;
 std::vector<std::uint32_t> stack{old.to},nodes;std::size_t tree_work=0;
 std::array<std::uint32_t,kWitnessProbeLimit> equal_candidates{};
 std::size_t equal_candidate_count=0;
 bool witness_probe_ran=false;
 const auto target=s.dist[old.to];
 while(!stack.empty()){
  auto u=stack.back();stack.pop_back();if(mark[u]==epoch)continue;mark[u]=epoch;nodes.push_back(u);
  if(discovered_vertices)*discovered_vertices=nodes.size();
  if(work)work->discovered_vertices=nodes.size();
  if(nodes.size()>budget.max_discovery_vertices||(work&&work->total()>budget.max_total_work))return RepairResult::RebuildRequired;

  // One bounded probe, and only for a repair already known to be nontrivial.
  // A tight predecessor with strictly smaller distance cannot be a descendant of
  // the affected root under nonnegative weights, so it is immediately safe. A
  // same-distance predecessor may belong to an undiscovered zero-weight part of
  // the subtree; retain it and decide only after discovery is complete.
  if(!witness_probe_ran&&nodes.size()>=kWitnessDiscoveryTrigger){
   witness_probe_ran=true;
   const auto incoming=g.in(old.to);
   const auto limit=std::min<std::size_t>(incoming.size(),kWitnessProbeLimit);
   for(std::size_t i=0;i<limit;++i){
    const auto a=incoming[i];
    if(work)work->witness_scans++;
    if(a.edge_id==id||s.dist[a.to]>=INF)continue;
    const auto&e=g.edge(a.edge_id);
    if(sat_add(s.dist[a.to],e.weight)!=target)continue;
    if(s.dist[a.to]<target){
     set_parent(g,s,old.to,a.edge_id);
     if(work)work->witness_rescues++;
     return RepairResult::Repaired;
    }
    if(equal_candidate_count<equal_candidates.size())equal_candidates[equal_candidate_count++]=a.edge_id;
   }
  }

  for(auto child=s.first_child[u];child!=kNoIndex;child=s.next_sibling[child]){
   ++tree_work;if(work)work->tree_edges++;
   if(tree_work>budget.max_discovery_tree_edges||(work&&work->total()>budget.max_total_work))return RepairResult::RebuildRequired;
   stack.push_back(child);
  }
 }

 // Resolve retained equal-distance witnesses only now that membership is known
 // for the complete affected subtree. A candidate outside it retains a selected
 // source witness independent of the increased edge and can safely reparent the
 // root. No second incoming-edge scan is paid.
 if(witness_probe_ran){
  for(std::size_t i=0;i<equal_candidate_count;++i){
   const auto candidate=equal_candidates[i];const auto&e=g.edge(candidate);
   if(mark[e.from]==epoch||s.dist[e.from]>=INF)continue;
   if(sat_add(s.dist[e.from],e.weight)!=target)continue;
   set_parent(g,s,old.to,candidate);
   if(work)work->witness_rescues++;
   return RepairResult::Repaired;
  }
 }

 for(auto v:nodes){s.dist[v]=INF;set_parent(g,s,v,kNoIndex);}
 RadixQueue q;
 for(auto v:nodes){
  for(auto a:g.in(v)){
   if(work){work->boundary_scans++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}
   if(mark[a.to]!=epoch&&s.dist[a.to]<INF){
    auto nd=sat_add(s.dist[a.to],g.edge(a.edge_id).weight);
    if(nd<s.dist[v]){s.dist[v]=nd;set_parent(g,s,v,a.edge_id);}
   }
  }
 }
 for(auto v:nodes)if(s.dist[v]<INF)q.push(s.dist[v],v);
 while(!q.empty()){
  auto[du,u]=q.pop();if(work){work->pq_pops++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}if(du!=s.dist[u])continue;
  for(auto a:g.out(u)){
   if(work){work->restricted_scans++;if(work->total()>budget.max_total_work)return RepairResult::RebuildRequired;}
   if(mark[a.to]==epoch){
    auto nd=sat_add(du,g.edge(a.edge_id).weight);
    if(nd<s.dist[a.to]){s.dist[a.to]=nd;set_parent(g,s,a.to,a.edge_id);q.push(nd,a.to);}
   }
  }
 }
 return RepairResult::Repaired;
}
}
