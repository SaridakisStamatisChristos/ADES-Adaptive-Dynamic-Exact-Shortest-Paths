#pragma once
#include "ades/graph.hpp"
#include <vector>
namespace ades {
struct SSSPState { std::uint32_t source{}; std::vector<Distance> dist; std::vector<std::int64_t> parent_edge; };
SSSPState dijkstra(const Graph&,std::uint32_t source);
Distance bidirectional_dijkstra(const Graph&,std::uint32_t source,std::uint32_t target);
}