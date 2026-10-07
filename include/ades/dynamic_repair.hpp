#pragma once
#include "ades/shortest_paths.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
namespace ades {
struct RepairBudget {
 std::size_t max_discovery_vertices=4096;
 std::size_t max_discovery_tree_edges=16384;
 std::uint64_t max_total_work=std::numeric_limits<std::uint64_t>::max();
};
struct RepairWork {
 std::uint64_t discovered_vertices=0,tree_edges=0,boundary_scans=0,restricted_scans=0,pq_pops=0;
 std::uint64_t total()const{return discovered_vertices+tree_edges+boundary_scans+restricted_scans+pq_pops;}
};
enum class RepairResult { Filtered, Repaired, RebuildRequired };
RepairResult repair_decrease(const Graph&, SSSPState&, std::uint32_t edge_id);
RepairResult repair_increase(const Graph&, SSSPState&, std::uint32_t edge_id, const Edge& old_edge, RepairBudget, std::size_t* discovered_vertices=nullptr, RepairWork* work=nullptr);
}