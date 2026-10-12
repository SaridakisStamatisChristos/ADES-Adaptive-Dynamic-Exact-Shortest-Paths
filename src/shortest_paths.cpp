#include "ades/shortest_paths.hpp"
#include "ades/radix_queue.hpp"
#include <algorithm>
#include <vector>
namespace ades {
namespace {
struct BidirectionalWorkspace {
 std::vector<Distance> df,db;
 std::vector<std::uint32_t> mf,mb;
 RadixQueue qf,qb;
 std::uint32_t epoch=0;
 void reset(std::size_t n){
  if(df.size()<n){df.resize(n);db.resize(n);mf.resize(n,0);mb.resize(n,0);}
  ++epoch;
  if(epoch==0){std::fill(mf.begin(),mf.end(),0);std::fill(mb.begin(),mb.end(),0);epoch=1;}
  qf.clear();qb.clear();
 }
 Distance forward(std::uint32_t v)const{return mf[v]==epoch?df[v]:INF;}
 Distance backward(std::uint32_t v)const{return mb[v]==epoch?db[v]:INF;}
 void set_forward(std::uint32_t v,Distance d){mf[v]=epoch;df[v]=d;}
 void set_backward(std::uint32_t v,Distance d){mb[v]=epoch;db[v]=d;}
};
thread_local BidirectionalWorkspace bidirectional_workspace;
}
void set_parent(const Graph& g,SSSPState& s,std::uint32_t v,std::uint32_t edge_id){
 auto old=s.parent_edge[v];if(old==edge_id)return;
 if(old!=kNoIndex){
  auto p=g.edge(old).from;auto prev=s.prev_sibling[v],next=s.next_sibling[v];
  if(prev!=kNoIndex)s.next_sibling[prev]=next;else s.first_child[p]=next;
  if(next!=kNoIndex)s.prev_sibling[next]=prev;
 }
 s.parent_edge[v]=edge_id;s.prev_sibling[v]=kNoIndex;s.next_sibling[v]=kNoIndex;
 if(edge_id!=kNoIndex){
  auto p=g.edge(edge_id).from;auto first=s.first_child[p];s.next_sibling[v]=first;
  if(first!=kNoIndex)s.prev_sibling[first]=v;
  s.first_child[p]=v;
 }
}
SSSPState dijkstra(const Graph& g,std::uint32_t s){
 const auto n=g.vertex_count();
 SSSPState r{s,std::vector<Distance>(n,INF),std::vector<std::uint32_t>(n,kNoIndex),
  std::vector<std::uint32_t>(n,kNoIndex),std::vector<std::uint32_t>(n,kNoIndex),std::vector<std::uint32_t>(n,kNoIndex),
  {},0};
 RadixQueue q;
 r.dist[s]=0;q.push(0,s);
 while(!q.empty()){auto [du,u]=q.pop();if(du!=r.dist[u])continue;
  for(auto a:g.out(u)){auto&e=g.edge(a.edge_id);auto nd=sat_add(du,e.weight);
   if(nd<r.dist[a.to]){r.dist[a.to]=nd;set_parent(g,r,a.to,a.edge_id);q.push(nd,a.to);}
  }}
 return r;
}
BidirectionalResult bidirectional_dijkstra_profiled(const Graph& g,std::uint32_t s,std::uint32_t t){
 BidirectionalResult result;if(s==t){result.distance=0;return result;}
 auto&ws=bidirectional_workspace;ws.reset(g.vertex_count());
 ws.set_forward(s,0);ws.set_backward(t,0);ws.qf.push(0,s);ws.qb.push(0,t);Distance best=INF;
 while(!ws.qf.empty()&&!ws.qb.empty()){
  const auto min_f=ws.qf.min_key(),min_b=ws.qb.min_key();
  if(sat_add(min_f,min_b)>=best)break;
  if(min_f<=min_b){auto [du,u]=ws.qf.pop();if(du!=ws.forward(u))continue;result.settled++;
   const auto bu=ws.backward(u);if(bu<INF)best=std::min(best,sat_add(du,bu));
   for(auto a:g.out(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<ws.forward(a.to)){ws.set_forward(a.to,nd);ws.qf.push(nd,a.to);}const auto b=ws.backward(a.to);if(b<INF)best=std::min(best,sat_add(nd,b));}
  }else{auto [du,u]=ws.qb.pop();if(du!=ws.backward(u))continue;result.settled++;
   const auto fu=ws.forward(u);if(fu<INF)best=std::min(best,sat_add(du,fu));
   for(auto a:g.in(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<ws.backward(a.to)){ws.set_backward(a.to,nd);ws.qb.push(nd,a.to);}const auto f=ws.forward(a.to);if(f<INF)best=std::min(best,sat_add(nd,f));}
  }}
 result.distance=best;return result;
}
}
