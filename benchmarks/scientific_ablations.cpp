#include "ades/ades.hpp"
#include "ades/memory_budget.hpp"
#include "ades/telemetry.hpp"
#include "ades/trace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ades;
using Clock=std::chrono::steady_clock;

struct Op{bool update;std::uint32_t a,b;Weight w,old_w;};
struct RunTiming{
 std::uint64_t algorithm_ns=0,query_ns=0,update_ns=0;
 std::vector<std::uint64_t> all,queries,updates;
};

static std::uint64_t elapsed_ns(Clock::time_point start){
 return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
}

static std::uint64_t percentile(std::vector<std::uint64_t> values,double p){
 if(values.empty())return 0;
 std::sort(values.begin(),values.end());
 const auto rank=static_cast<std::size_t>(std::ceil(p*double(values.size())));
 return values[std::min(values.size()-1,rank?rank-1:std::size_t{0})];
}

static std::vector<std::uint32_t> hot_pool(std::size_t n,std::size_t k,std::mt19937_64&r){
 std::vector<std::uint32_t> v(n);
 for(std::uint32_t i=0;i<n;i++)v[i]=i;
 std::shuffle(v.begin(),v.end(),r);
 v.resize(std::min(k,n));
 return v;
}

static std::uint32_t pick_source(const std::string&family,const std::vector<std::uint32_t>&hot,
                                 std::size_t q,std::size_t epoch,std::size_t n,std::mt19937_64&r){
 if(family=="uniform")return r()%n;
 if(family=="single-hot")return (r()%100<95)?hot[0]:r()%n;
 if(family=="rotating-hot")return (r()%100<90)?hot[(q/epoch)%hot.size()]:r()%n;
 if(family=="hot-pool")return (r()%100<90)?hot[r()%hot.size()]:r()%n;
 if(family=="churn")return hot[(q/epoch)%hot.size()];
 if(family=="zipf"){
  double u=std::generate_canonical<double,53>(r),h=0,c=0;
  for(std::size_t i=1;i<=hot.size();++i)h+=1.0/double(i);
  for(std::size_t i=1;i<=hot.size();++i){c+=(1.0/double(i))/h;if(u<=c)return hot[i-1];}
  return hot.back();
 }
 throw std::runtime_error("unknown source family");
}

static std::uint32_t directional_edge(const Graph&g,std::uint32_t start,bool increase){
 constexpr Weight max_w=std::numeric_limits<Weight>::max();
 for(std::size_t i=0;i<g.edge_count();++i){
  auto id=static_cast<std::uint32_t>((std::size_t(start)+i)%g.edge_count());
  const auto w=g.edge(id).weight;
  if(increase?(w<=max_w-31):(w>0))return id;
 }
 throw std::runtime_error(increase?"no safely increasable edge":"no positive edge");
}

static std::vector<Op> make_trace(const Graph&input,const std::string&family,std::uint64_t seed,
                                  std::size_t query_count,std::size_t update_every,
                                  std::size_t hot_sources,std::size_t epoch,const std::string&update_mode){
 Graph g=input;
 std::mt19937_64 r(seed);
 auto hot=hot_pool(g.vertex_count(),std::max<std::size_t>(1,hot_sources),r);
 std::vector<Op> ops;
 ops.reserve(query_count+(update_every?query_count/update_every:0));
 std::size_t update_index=0;
 for(std::size_t q=0;q<query_count;++q){
  if(update_every&&q&&q%update_every==0){
   auto id=static_cast<std::uint32_t>(r()%g.edge_count());
   auto old=g.edge(id).weight;
   Weight nw=old;
   if(update_mode=="random"){
    if(r()&1)nw=old+1+(r()%31);
    else if(old){const auto bound=std::min<Weight>(old,31);nw=old-(r()%(bound+1));}
   }else if(update_mode=="alternating"){
    const bool increase=(update_index%2)==0;
    id=directional_edge(g,id,increase);old=g.edge(id).weight;
    if(increase)nw=old+1+(r()%31);
    else{const auto bound=std::min<Weight>(old,31);nw=old-(1+(r()%bound));}
   }else throw std::runtime_error("update mode must be random or alternating");
   ++update_index;
   g.update_weight(id,nw);
   ops.push_back({true,id,0,nw,old});
  }
  const auto s=pick_source(family,hot,q,std::max<std::size_t>(1,epoch),g.vertex_count(),r);
  const auto t=static_cast<std::uint32_t>(r()%g.vertex_count());
  ops.push_back({false,s,t,0,0});
 }
 return ops;
}

static std::vector<TraceOp> canonicalize(const std::vector<Op>&ops){
 std::vector<TraceOp> trace;trace.reserve(ops.size());
 for(const auto&o:ops)trace.push_back(o.update?TraceOp::update(o.a,o.old_w,o.w):TraceOp::query(o.a,o.b));
 return trace;
}

static std::pair<std::vector<Distance>,std::uint64_t> oracle(Graph g,const std::vector<Op>&ops){
 std::vector<Distance> answers;answers.reserve(ops.size());
 const auto start=Clock::now();
 for(const auto&o:ops){if(o.update)g.update_weight(o.a,o.w);else answers.push_back(dijkstra(g,o.a).dist[o.b]);}
 return {std::move(answers),elapsed_ns(start)};
}

static RunTiming run(ADES&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 RunTiming rt;rt.all.reserve(ops.size());rt.queries.reserve(ref.size());rt.updates.reserve(ops.size()-ref.size());
 std::size_t qi=0;
 for(const auto&o:ops){
  const auto start=Clock::now();
  if(o.update){
   e.update(o.a,o.w);const auto ns=elapsed_ns(start);rt.update_ns+=ns;rt.updates.push_back(ns);rt.all.push_back(ns);
  }else{
   const auto answer=e.query(o.a,o.b);const auto ns=elapsed_ns(start);rt.query_ns+=ns;rt.queries.push_back(ns);rt.all.push_back(ns);
   if(answer!=ref.at(qi++))throw std::runtime_error("ablation exactness failure");
  }
 }
 rt.algorithm_ns=rt.query_ns+rt.update_ns;
 return rt;
}

static std::uint64_t persistent_budget(){
 const char*raw=std::getenv("ADES_PERSISTENT_STATE_BUDGET_BYTES");
 if(!raw||!*raw)return 0;
 std::string value(raw);std::size_t pos=0;std::uint64_t budget=0;
 try{budget=std::stoull(value,&pos);}catch(const std::exception&){throw std::runtime_error("invalid ADES_PERSISTENT_STATE_BUDGET_BYTES");}
 if(pos!=value.size()||!budget)throw std::runtime_error("invalid ADES_PERSISTENT_STATE_BUDGET_BYTES");
 return budget;
}

static void emit(const char*name,const std::string&family,std::uint64_t seed,const std::string&sha,
                 const TraceCounts&counts,std::size_t update_every,std::size_t hot_sources,std::size_t epoch,
                 std::size_t cap,std::uint64_t oracle_ns,const RunTiming&rt,const Stats&st){
 const auto&t=st.timing;
 std::cout<<kPublicationTelemetrySchemaVersion<<','<<name<<','<<family<<','<<seed<<','<<sha<<','
          <<counts.query_count<<','<<counts.update_count<<','<<counts.increase_count<<','<<counts.decrease_count<<','
          <<update_every<<','<<hot_sources<<','<<epoch<<','<<cap<<','
          <<st.persistent_state_budget_bytes<<','<<st.accounted_algorithm_state_bytes<<','
          <<st.peak_accounted_algorithm_state_bytes<<','<<kPersistentStateAccountingVersion<<','
          <<oracle_ns<<','<<rt.algorithm_ns<<','<<rt.query_ns<<','<<rt.update_ns<<','
          <<percentile(rt.all,0.50)<<','<<percentile(rt.all,0.95)<<','
          <<percentile(rt.queries,0.50)<<','<<percentile(rt.queries,0.95)<<','
          <<percentile(rt.updates,0.50)<<','<<percentile(rt.updates,0.95)<<','
          <<st.cold_queries<<','<<st.resident_queries<<','<<st.promotions<<','<<st.evictions<<','
          <<st.rebuilds<<','<<st.repair_aborts<<','<<st.cooldown_blocks<<','<<st.admission_rejections<<','
          <<st.filtered_updates<<','<<st.decrease_repairs<<','<<st.increase_repairs<<','
          <<st.memory_budget_rejections<<','<<st.memory_metadata_prunes<<','
          <<t.query_time_ns<<','<<t.update_time_ns<<','<<t.cold_query_ns<<','<<t.resident_query_ns<<','
          <<t.query_policy_ns<<','<<t.promotion_ns<<','<<t.eviction_ns<<','<<t.graph_update_ns<<','
          <<t.decrease_repair_ns<<','<<t.increase_repair_ns<<','<<t.rebuild_ns<<','<<t.controller_ns<<','
          <<t.update_policy_ns<<'\n';
}

int main(int argc,char**argv){
 try{
  if(argc<8){
   std::cerr<<"usage: ades_scientific_ablations graph family queries update_every hot_sources epoch seed [cap] [random|alternating]\n";
   return 2;
  }
  auto g=Graph::load_dimacs_gr_gz(argv[1]);
  const std::string family=argv[2];
  const auto query_count=std::strtoull(argv[3],nullptr,10);
  const auto update_every=std::strtoull(argv[4],nullptr,10);
  const auto hot_sources=std::strtoull(argv[5],nullptr,10);
  const auto epoch=std::strtoull(argv[6],nullptr,10);
  const std::uint64_t seed=std::strtoull(argv[7],nullptr,10);
  const std::size_t requested_cap=argc>8?std::strtoull(argv[8],nullptr,10):8;
  const std::string update_mode=argc>9?argv[9]:"alternating";
  const auto budget=persistent_budget();
  const std::size_t effective_cap=budget?std::max<std::size_t>(1,g.vertex_count()):requested_cap;

  auto ops=make_trace(g,family,seed,query_count,update_every,hot_sources,epoch,update_mode);
  auto canonical=canonicalize(ops);
  validate_trace_against_graph(g,canonical);
  const auto sha=trace_sha256(canonical);
  const auto counts=trace_counts(canonical);
  auto [ref,oracle_ns]=oracle(g,ops);

  const ScientificAblation profiles[]={
   ScientificAblation::Cold,
   ScientificAblation::FreqLruRebuild,
   ScientificAblation::FreqLruRepair,
   ScientificAblation::WorkLruRepair,
   ScientificAblation::WorkDebtRepair,
   ScientificAblation::FullADES,
  };

  for(const auto profile:profiles){
   Config base;base.resident_cap=effective_cap;base.persistent_state_budget_bytes=budget;
   auto cfg=config_for_scientific_ablation(profile,base);
   ADES engine(g,cfg);
   auto rt=run(engine,ops,ref);
   const auto&st=engine.stats();
   if(rt.algorithm_ns!=rt.query_ns+rt.update_ns)throw std::logic_error("top-level timing does not reconcile");
   if(!st.timing.reconciles())throw std::logic_error("ADES internal timing does not reconcile");
   if(budget&&st.peak_accounted_algorithm_state_bytes>budget)throw std::logic_error("ablation exceeded persistent-state budget");
   emit(scientific_ablation_name(profile),family,seed,sha,counts,update_every,hot_sources,epoch,
        cfg.resident_cap,oracle_ns,rt,st);
   std::cerr<<"ABLATION profile="<<scientific_ablation_name(profile)
            <<" trace_sha256="<<sha
            <<" cold="<<st.cold_queries
            <<" resident="<<st.resident_queries
            <<" promotions="<<st.promotions
            <<" evictions="<<st.evictions
            <<" rebuilds="<<st.rebuilds
            <<" repair_aborts="<<st.repair_aborts
            <<" peak_accounted_bytes="<<st.peak_accounted_algorithm_state_bytes<<'\n';
  }
  return 0;
 }catch(const std::exception&e){
  std::cerr<<"ades_scientific_ablations: "<<e.what()<<'\n';
  return 2;
 }
}
