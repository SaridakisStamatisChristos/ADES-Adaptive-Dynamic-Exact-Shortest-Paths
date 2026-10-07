#include "ades/shortest_paths.hpp"
#include <algorithm>
#include <functional>
#include <queue>
namespace ades {
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
 using P=std::pair<Distance,std::uint32_t>;std::priority_queue<P,std::vector<P>,std::greater<P>> q;
 r.dist[s]=0;q.push({0,s});
 while(!q.empty()){auto [du,u]=q.top();q.pop();if(du!=r.dist[u])continue;
  for(auto a:g.out(u)){auto&e=g.edge(a.edge_id);auto nd=sat_add(du,e.weight);
   if(nd<r.dist[a.to]){r.dist[a.to]=nd;set_parent(g,r,a.to,a.edge_id);q.push({nd,a.to});}
  }}
 return r;
}
BidirectionalResult bidirectional_dijkstra_profiled(const Graph& g,std::uint32_t s,std::uint32_t t){
 BidirectionalResult result;if(s==t){result.distance=0;return result;}
 using P=std::pair<Distance,std::uint32_t>;
 std::vector<Distance> df(g.vertex_count(),INF),db(g.vertex_count(),INF);
 std::priority_queue<P,std::vector<P>,std::greater<P>> qf,qb;df[s]=0;db[t]=0;qf.push({0,s});qb.push({0,t});Distance best=INF;
 while(!qf.empty()&&!qb.empty()){
  if(sat_add(qf.top().first,qb.top().first)>=best)break;
  if(qf.top().first<=qb.top().first){auto [du,u]=qf.top();qf.pop();if(du!=df[u])continue;result.settled++;
   if(db[u]<INF)best=std::min(best,sat_add(du,db[u]));
   for(auto a:g.out(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<df[a.to]){df[a.to]=nd;qf.push({nd,a.to});}if(db[a.to]<INF)best=std::min(best,sat_add(nd,db[a.to]));}
  }else{auto [du,u]=qb.top();qb.pop();if(du!=db[u])continue;result.settled++;
   if(df[u]<INF)best=std::min(best,sat_add(du,df[u]));
   for(auto a:g.in(u)){result.edge_scans++;auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<db[a.to]){db[a.to]=nd;qb.push({nd,a.to});}if(df[a.to]<INF)best=std::min(best,sat_add(nd,df[a.to]));}
  }}
 result.distance=best;return result;
}
}
