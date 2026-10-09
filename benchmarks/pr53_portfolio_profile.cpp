#include "ades/portfolio_ades.hpp"
#include "ades/trace.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ades;
using Clock = std::chrono::steady_clock;

static std::uint64_t elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

static std::vector<Distance> read_oracle(const std::string& path,
                                         const std::string& expected_sha,
                                         std::size_t expected_queries) {
  std::ifstream in(path, std::ios::binary);
  std::string magic, sha; std::size_t count=0;
  if(!(in>>magic>>sha>>count)||magic!="ADES_ORACLE_V1"||sha!=expected_sha||count!=expected_queries)
    throw std::runtime_error("portfolio oracle identity mismatch");
  std::vector<Distance> out(count); for(auto& d:out) if(!(in>>d)) throw std::runtime_error("truncated oracle");
  return out;
}

int main(int argc,char**argv){
 try{
  if(argc!=6) throw std::invalid_argument("args: graph trace oracle budget profile");
  auto graph=Graph::load_dimacs_gr_gz(argv[1]); auto ops=read_trace(argv[2]); validate_trace_against_graph(graph,ops);
  const auto tsha=trace_sha256(ops); const auto counts=trace_counts(ops); auto oracle=read_oracle(argv[3],tsha,counts.query_count);
  const auto budget=std::stoull(argv[4]); if(std::string(argv[5])!="ADES-V5P") throw std::invalid_argument("profile must be ADES-V5P");
  HazardConfig cfg; cfg.resident_cap=4; cfg.partial_cap=8; cfg.persistent_state_budget_bytes=budget;
  PortfolioADES engine(std::move(graph),cfg); std::size_t qi=0; std::uint64_t qns=0,uns=0; const auto all=Clock::now();
  for(const auto&op:ops){const auto s=Clock::now(); if(op.kind==TraceOpKind::Update){engine.update(op.a,op.new_weight);uns+=elapsed_ns(s);}else{auto got=engine.query(op.a,op.b);qns+=elapsed_ns(s);if(qi>=oracle.size()||got!=oracle[qi++])throw std::runtime_error("portfolio exactness failure");}}
  if(qi!=oracle.size())throw std::runtime_error("oracle consumption mismatch");
  const auto total=elapsed_ns(all); const auto peak=engine.peak_accounted_algorithm_state_bytes(); if(peak>budget)throw std::runtime_error("portfolio budget violation");
  const auto&s=engine.stats();
  std::cout<<"ADES-V5P,"<<tsha<<','<<counts.query_count<<','<<counts.update_count<<','<<budget<<','<<total<<','<<qns<<','<<uns
           <<",0,0,"<<engine.accounted_algorithm_state_bytes()<<','<<peak<<','<<s.cold_queries<<','<<s.resident_queries<<','<<s.promotions<<','<<s.evictions<<','<<s.rebuilds
           <<",0,0,0,0,0,0,0,0\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"ades_pr53_portfolio_profile: "<<e.what()<<'\n';return 2;}
}
