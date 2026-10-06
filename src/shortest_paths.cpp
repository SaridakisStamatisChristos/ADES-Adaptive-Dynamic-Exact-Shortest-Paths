#include "ades/shortest_paths.hpp"
#include <functional>
#include <queue>
namespace ades {
SSSPState dijkstra(const Graph& g,std::uint32_t s){
 SSSPState r{s,std::vector<Distance>(g.vertex_count(),INF),std::vector<std::int64_t>(g.vertex_count(),-1)};
 using P=std::pair<Distance,std::uint32_t>; std::priority_queue<P,std::vector<P>,std::greater<P>> q;
 r.dist[s]=0;q.push({0,s});
 while(!q.empty()){auto [du,u]=q.top();q.pop();if(du!=r.dist[u])continue;
  for(auto a:g.out(u)){auto &e=g.edge(a.edge_id);auto nd=sat_add(du,e.weight);
   if(nd<r.dist[a.to]){r.dist[a.to]=nd;r.parent_edge[a.to]=a.edge_id;q.push({nd,a.to});}
  }}
 return r;
}
Distance bidirectional_dijkstra(const Graph& g,std::uint32_t s,std::uint32_t t){
 if(s==t) return 0;
 using P=std::pair<Distance,std::uint32_t>;
 std::vector<Distance> df(g.vertex_count(),INF),db(g.vertex_count(),INF);
 std::priority_queue<P,std::vector<P>,std::greater<P>> qf,qb;df[s]=0;db[t]=0;qf.push({0,s});qb.push({0,t});Distance best=INF;
 while(!qf.empty()&&!qb.empty()){
  if(sat_add(qf.top().first,qb.top().first)>=best)break;
  if(qf.top().first<=qb.top().first){auto [du,u]=qf.top();qf.pop();if(du!=df[u])continue;
   if(db[u]<INF)best=std::min(best,sat_add(du,db[u]));
   for(auto a:g.out(u)){auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<df[a.to]){df[a.to]=nd;qf.push({nd,a.to});}if(db[a.to]<INF)best=std::min(best,sat_add(nd,db[a.to]));}
  }else{auto [du,u]=qb.top();qb.pop();if(du!=db[u])continue;
   if(df[u]<INF)best=std::min(best,sat_add(du,df[u]));
   for(auto a:g.in(u)){auto nd=sat_add(du,g.edge(a.edge_id).weight);if(nd<db[a.to]){db[a.to]=nd;qb.push({nd,a.to});}if(df[a.to]<INF)best=std::min(best,sat_add(nd,df[a.to]));}
  }}
 return best;
}
}