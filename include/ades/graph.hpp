#pragma once
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace ades {
using Weight=std::uint64_t; using Distance=std::uint64_t;
inline constexpr Distance INF=std::numeric_limits<Distance>::max()/4;
struct Edge { std::uint32_t from,to; Weight weight; };
struct Arc { std::uint32_t to, edge_id; };
class Graph {
 std::size_t vertex_count_=0;
 std::vector<Edge> edges_;
 std::vector<std::vector<Arc>> out_,in_;
 std::vector<Arc> out_flat_,in_flat_;
 std::vector<std::size_t> out_offsets_,in_offsets_;
 bool frozen_=false;
public:
 explicit Graph(std::size_t n=0):vertex_count_(n),out_(n),in_(n){}
 std::size_t vertex_count()const{return vertex_count_;}
 std::size_t edge_count()const{return edges_.size();}
 std::uint32_t add_edge(std::uint32_t u,std::uint32_t v,Weight w);
 void freeze_topology();
 bool topology_frozen()const noexcept{return frozen_;}
 void update_weight(std::uint32_t id,Weight w){edges_.at(id).weight=w;}
 const Edge& edge(std::uint32_t id)const{return edges_.at(id);}
 std::span<const Arc> out(std::uint32_t u)const{
  if(u>=vertex_count_)throw std::out_of_range("vertex");
  if(!frozen_)return std::span<const Arc>(out_[u]);
  return std::span<const Arc>(out_flat_).subspan(out_offsets_[u],out_offsets_[u+1]-out_offsets_[u]);
 }
 std::span<const Arc> in(std::uint32_t v)const{
  if(v>=vertex_count_)throw std::out_of_range("vertex");
  if(!frozen_)return std::span<const Arc>(in_[v]);
  return std::span<const Arc>(in_flat_).subspan(in_offsets_[v],in_offsets_[v+1]-in_offsets_[v]);
 }
 static Graph load_dimacs_gr_gz(const std::string& path);
};
inline Distance sat_add(Distance a,Weight b){return a>=INF||b>=INF-a?INF:a+b;}
}