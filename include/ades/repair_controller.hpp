#pragma once
#include "ades/dynamic_repair.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace ades {
class RepairController {
 double alpha_=0.20,gamma_=0.90;
 double rebuild_ns_=0.0,repair_ns_per_work_=0.0,repair_ns_per_vertex_=0.0,work_per_vertex_=5.0,tree_edges_per_vertex_=4.0;
 std::size_t hard_vertices_=1u<<20,hard_tree_edges_=1u<<22;
 static void ewma(double& x,double v,double a){x=x==0.0?v:(1.0-a)*x+a*v;}
public:
 struct Budget { std::size_t vertices,tree_edges; std::uint64_t work_units; };
 RepairController()=default;
 RepairController(double alpha,double gamma):alpha_(alpha),gamma_(gamma){}
 void observe_rebuild(std::uint64_t ns){ewma(rebuild_ns_,double(ns),alpha_);}
 void observe_repair(std::uint64_t ns,const RepairWork& work){
  const auto units=work.total();
  if(units)ewma(repair_ns_per_work_,double(ns)/double(units),alpha_);
  if(work.discovered_vertices)ewma(repair_ns_per_vertex_,double(ns)/double(work.discovered_vertices),alpha_);
  if(work.discovered_vertices){ewma(work_per_vertex_,double(units)/double(work.discovered_vertices),alpha_);ewma(tree_edges_per_vertex_,double(work.tree_edges)/double(work.discovered_vertices),alpha_);}
 }
 Budget budget()const{
  if(rebuild_ns_<=0.0||repair_ns_per_work_<=0.0)return {4096,16384,32768};
  const double target_work=gamma_*rebuild_ns_/repair_ns_per_work_;
  auto wu=std::uint64_t(std::max(1.0,target_work));
  auto v=std::size_t(std::max(1.0,target_work/std::max(1.0,work_per_vertex_)));v=std::min(v,hard_vertices_);
  auto e=std::size_t(std::max(1.0,double(v)*std::max(1.0,tree_edges_per_vertex_)));
  return {v,std::min(e,hard_tree_edges_),wu};
 }
 Budget vertex_only_budget()const{
  if(rebuild_ns_<=0.0||repair_ns_per_vertex_<=0.0)return {4096,hard_tree_edges_,std::numeric_limits<std::uint64_t>::max()};
  auto v=std::size_t(std::max(1.0,gamma_*rebuild_ns_/repair_ns_per_vertex_));
  return {std::min(v,hard_vertices_),hard_tree_edges_,std::numeric_limits<std::uint64_t>::max()};
 }
 double rebuild_estimate_ns()const{return rebuild_ns_;}
 double repair_work_estimate_ns()const{return repair_ns_per_work_;}
 double work_per_vertex_estimate()const{return work_per_vertex_;}
 double tree_edges_per_vertex_estimate()const{return tree_edges_per_vertex_;}
};
}