#pragma once
#include "ades/dynamic_repair.hpp"
#include <unordered_map>
namespace ades {
struct Config { std::size_t resident_cap=4; std::uint32_t probation_queries=4; double promotion_ratio=1.05; RepairBudget repair_budget{}; };
struct Stats { std::uint64_t cold_queries=0,resident_queries=0,promotions=0,rebuilds=0,filtered_updates=0,decrease_repairs=0,increase_repairs=0,repair_aborts=0; };
class ADES {
 Graph graph_; Config cfg_; Stats stats_;
 struct Entry { SSSPState s; std::uint64_t hits=0; };
 std::unordered_map<std::uint32_t,Entry> residents_;
 std::unordered_map<std::uint32_t,std::uint32_t> probation_;
 void rebuild(std::uint32_t source);
public:
 explicit ADES(Graph g,Config c={}):graph_(std::move(g)),cfg_(c){}
 Distance query(std::uint32_t s,std::uint32_t t);
 void update(std::uint32_t edge_id,Weight new_weight);
 const Graph& graph()const{return graph_;}
 const Stats& stats()const{return stats_;}
 bool resident(std::uint32_t s)const{return residents_.contains(s);}
};
}
