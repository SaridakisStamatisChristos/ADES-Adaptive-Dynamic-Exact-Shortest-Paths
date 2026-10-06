#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
namespace ades {
class RepairController {
 double alpha_=0.20, gamma_=0.90;
 double rebuild_ns_=0.0, repair_ns_per_vertex_=0.0;
 std::size_t hard_vertices_=1u<<20, hard_tree_edges_=1u<<22;
 static void ewma(double& x,double v,double a){x=x==0.0?v:(1.0-a)*x+a*v;}
public:
 struct Budget { std::size_t vertices, tree_edges; };
 RepairController()=default;
 RepairController(double alpha,double gamma):alpha_(alpha),gamma_(gamma){}
 void observe_rebuild(std::uint64_t ns){ewma(rebuild_ns_,double(ns),alpha_);}
 void observe_repair(std::uint64_t ns,std::size_t vertices){
  if(vertices) ewma(repair_ns_per_vertex_,double(ns)/double(vertices),alpha_);
 }
 Budget budget()const{
  if(rebuild_ns_<=0.0||repair_ns_per_vertex_<=0.0) return {4096,16384};
  auto v=std::size_t(std::max(1.0,gamma_*rebuild_ns_/repair_ns_per_vertex_));
  v=std::min(v,hard_vertices_); return {v,std::min(hard_tree_edges_,v*4)};
 }
 double rebuild_estimate_ns()const{return rebuild_ns_;}
 double repair_vertex_estimate_ns()const{return repair_ns_per_vertex_;}
};
}
