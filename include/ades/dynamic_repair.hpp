#pragma once
#include "ades/shortest_paths.hpp"
#include <cstddef>
namespace ades {
struct RepairBudget { std::size_t max_discovery_vertices=4096; std::size_t max_discovery_tree_edges=16384; };
enum class RepairResult { Filtered, Repaired, RebuildRequired };
RepairResult repair_decrease(const Graph&, SSSPState&, std::uint32_t edge_id);
RepairResult repair_increase(const Graph&, SSSPState&, std::uint32_t edge_id, const Edge& old_edge, RepairBudget, std::size_t* discovered_vertices=nullptr);
}
