#pragma once
#include "ades/graph.hpp"
#include <vector>
namespace ades {
struct SSSPState {
 std::uint32_t source{};
 std::vector<Distance> dist;
 std::vector<std::int64_t> parent_edge;
 std::vector<std::int64_t> first_child, next_sibling, prev_sibling;
};
struct BidirectionalResult { Distance distance=INF; std::uint64_t edge_scans=0; std::uint64_t settled=0; };
SSSPState dijkstra(const Graph&,std::uint32_t source);
void set_parent(const Graph&,SSSPState&,std::uint32_t vertex,std::int64_t edge_id);
BidirectionalResult bidirectional_dijkstra_profiled(const Graph&,std::uint32_t source,std::uint32_t target);
inline Distance bidirectional_dijkstra(const Graph&g,std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra_profiled(g,s,t).distance;}
}
