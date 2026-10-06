#pragma once
#include "ades/graph.hpp"
#include <vector>
namespace ades {
struct SSSPState { std::uint32_t source{}; std::vector<Distance> dist; std::vector<std::int64_t> parent_edge; };
struct BidirectionalResult { Distance distance=INF; std::uint64_t edge_scans=0; std::uint64_t settled=0; };
SSSPState dijkstra(const Graph&,std::uint32_t source);
BidirectionalResult bidirectional_dijkstra_profiled(const Graph&,std::uint32_t source,std::uint32_t target);
inline Distance bidirectional_dijkstra(const Graph&g,std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra_profiled(g,s,t).distance;}
}
