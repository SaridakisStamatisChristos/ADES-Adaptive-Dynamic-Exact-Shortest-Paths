#pragma once
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
namespace ades {
using Weight=std::uint64_t; using Distance=std::uint64_t;
inline constexpr Distance INF=std::numeric_limits<Distance>::max()/4;
struct Edge { std::uint32_t from,to; Weight weight; };
struct Arc { std::uint32_t to, edge_id; };
class Graph {
 std::vector<Edge> edges_; std::vector<std::vector<Arc>> out_,in_;
public:
 explicit Graph(std::size_t n=0):out_(n),in_(n){}
 std::size_t vertex_count()const{return out_.size();}
 std::size_t edge_count()const{return edges_.size();}
 std::uint32_t add_edge(std::uint32_t u,std::uint32_t v,Weight w);
 void update_weight(std::uint32_t id,Weight w){edges_.at(id).weight=w;}
 const Edge& edge(std::uint32_t id)const{return edges_.at(id);}
 const auto& out(std::uint32_t u)const{return out_.at(u);}
 const auto& in(std::uint32_t v)const{return in_.at(v);}
 static Graph load_dimacs_gr_gz(const std::string& path);
};
inline Distance sat_add(Distance a,Weight b){return a>=INF||b>=INF-a?INF:a+b;}
}