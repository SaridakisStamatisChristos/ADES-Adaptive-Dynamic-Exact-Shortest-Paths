#include "ades/shortest_paths.hpp"
#include <algorithm>
#include <functional>
#include <queue>
#include <vector>
namespace ades {
namespace {
using P=std::pair<Distance,std::uint32_t>;
struct BidirectionalWorkspace {
 std::vector<Distance> df,db;
 std::vector<std::uint32_t> mf,mb;
 std::vector<P> qf,qb;
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
inline void heap_push(std::vector<P>&q,P value){q.push_back(value);std::push_heap(q.begin(),q.end(),std::greater<P>{});}
inline P heap_pop(std::vector<P>&q){std::pop_heap(q.begin(),q.end(),std::greater<P>{});auto value=q.back();q.pop_back();return value;}
}
void set_parent(const Graph& g,SSSPState& s,std::uint32_t v,std::int64_t edge_id){
 auto old=s.parent_edge[v];if(old==edge_id)return;
 if(old>=0){
  auto p=g.edge((std::uint32_t)old).from;auto prev=s.prev_sibling[v],next=s.next_sibling[v];
  if(prev>=0)s.next_sibling[(std::uint32_t)prev]=next;else s.first_child[p]=next;
  if(next>=0)s.prev_sibling[(std::uint32_t)next]=prev;
 }
 s.parent_edge[v]=edge_id;s.prev_sibling[v]=-1;s.next_sibling[v]=-1;
 if(edge_id>=0){
  auto p=g.edge((std::uint32_t)edge_id).from;auto first=s.first_child[p];s.next_sibling[v]=first;
  if(first>=0)s.prev_sibling[(std::uint32_t)first]=v;
  s.first_child[p]=v;
 }
}
SSSPState dijkstra(const Graph& g,std::uint32_t s){
 const auto n=g.vertex_count();
 SSSPState r{s,std::vector<Distance>(n,INF),std::vector<std::int64_t>(n,-1),
  std::vector<std::int64_t>(n,-1),std::vector<std::int64_t>(n,-1),std::vector<std::int64_t>(n,-1),
  std::vector<std::uint32_t>(n,0),0};
 std::priority_queue<P,std::vector<P>,std::greater<P>> q;
 r.dist[s]=0;q.push({0,s});
 while(!q.empty()){auto [du,u]=q.top();q.pop();if(du!=r.dist[u])continue;
  for(auto a:g.out(u)){auto&e=g.edge(a.edge_id);auto nd=sat_add(du,e.weight);
   if(nd<r.dist[a.to]){r.dist[a.to]=nd;set_parent(g,r,a.to,a.edge_id);q.push({nd,a.to});}
  }}
 return r;
}
BidirectionalResult bidirectional_dijkstra_profiled(const Graph& g,std::uint32_t s,std::uint32_t t){
 BidirectionalResult result;if(s==t){result.distance=0;return result;}
 auto&ws=bidirectional_workspace;ws.reset(g.vertex_count());
 ws.set_forward(s,0);ws.set_backward(t,0);heap_push(ws.qf,{0,s});heap_push(ws.qb,{0,t});Distance best=INF;
 while(!ws.qf.empty()&&!ws.qb.empty()){
  if(sat_add(ws.qf.front().first,ws.qb.front().first)>=best)break;
  if(ws.qf.front().first<=ws.qb.front().first){auto [du,u]=heap_pop(ws.qf);if(du!=ws.forward(u))continue;result.settled++;
   const auto bu=ws.backward(u);if(bu<INF)best=std::min(best,sat_add(du,bu));
   for(auto a:g.out(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<ws.forward(a.to)){ws.set_forward(a.to,nd);heap_push(ws.qf,{nd,a.to});}const auto b=ws.backward(a.to);if(b<INF)best=std::min(best,sat_add(nd,b));}
  }else{auto [du,u]=heap_pop(ws.qb);if(du!=ws.backward(u))continue;result.settled++;
   const auto fu=ws.forward(u);if(fu<INF)best=std::min(best,sat_add(du,fu));
   for(auto a:g.in(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<ws.backward(a.to)){ws.set_backward(a.to,nd);heap_push(ws.qb,{nd,a.to});}const auto f=ws.forward(a.to);if(f<INF)best=std::min(best,sat_add(nd,f));}
  }}
 result.distance=best;return result;
}
}
