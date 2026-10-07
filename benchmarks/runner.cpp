#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/coordinates.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ades;using Clock=std::chrono::steady_clock;
struct Op{bool update;std::uint32_t a,b;Weight w;};
static void write_oracle(const std::string&path,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 std::ofstream o(path);if(!o)throw std::runtime_error("cannot write oracle");o<<ops.size()<<" "<<ref.size()<<"\n";std::size_t q=0;
 for(auto&x:ops){if(x.update)o<<"u "<<x.a<<" "<<x.w<<"\n";else o<<"q "<<x.a<<" "<<x.b<<" "<<ref.at(q++)<<"\n";}
}
static std::pair<std::vector<Op>,std::vector<Distance>> read_oracle(const std::string&path){
 std::ifstream in(path);if(!in)throw std::runtime_error("cannot read oracle");std::size_t n=0,nq=0;if(!(in>>n>>nq))throw std::runtime_error("bad oracle header");
 std::vector<Op> ops;std::vector<Distance> ref;ops.reserve(n);ref.reserve(nq);char k;
 for(std::size_t i=0;i<n;i++){if(!(in>>k))throw std::runtime_error("truncated oracle");if(k=='u'){std::uint32_t id;Weight w;if(!(in>>id>>w))throw std::runtime_error("bad oracle update");ops.push_back({true,id,0,w});}else if(k=='q'){std::uint32_t s,t;Distance d;if(!(in>>s>>t>>d))throw std::runtime_error("bad oracle query");ops.push_back({false,s,t,0});ref.push_back(d);}else throw std::runtime_error("bad oracle op");}
 if(ref.size()!=nq)throw std::runtime_error("oracle query-count mismatch");return {std::move(ops),std::move(ref)};
}
static std::vector<Op> mixed_trace(const Graph&g,std::uint64_t seed,std::size_t n){
 std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);for(std::size_t i=0;i<n;i++){
  if(g.edge_count()&&r()%4==0)x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});
  else x.push_back({false,(std::uint32_t)(r()%g.vertex_count()),(std::uint32_t)(r()%g.vertex_count()),0});}return x;
}
struct SpatialGrid{
 std::vector<std::vector<std::uint32_t>> cells;std::int64_t minx,miny,dx,dy;std::size_t side=32;
 explicit SpatialGrid(const std::vector<Coordinate>&c):cells(side*side){
  auto [xmin,xmax]=std::minmax_element(c.begin(),c.end(),[](auto&a,auto&b){return a.x<b.x;});
  auto [ymin,ymax]=std::minmax_element(c.begin(),c.end(),[](auto&a,auto&b){return a.y<b.y;});
  minx=xmin->x;miny=ymin->y;dx=std::max<std::int64_t>(1,std::int64_t(xmax->x)-minx+1);dy=std::max<std::int64_t>(1,std::int64_t(ymax->y)-miny+1);
  for(std::uint32_t v=0;v<c.size();v++){auto ix=std::min<std::size_t>(side-1,(std::uint64_t(std::int64_t(c[v].x)-minx)*side)/dx);auto iy=std::min<std::size_t>(side-1,(std::uint64_t(std::int64_t(c[v].y)-miny)*side)/dy);cells[iy*side+ix].push_back(v);}
 }
 std::size_t nonempty(std::size_t start,std::size_t step=1)const{for(std::size_t k=0;k<cells.size();k++){auto i=(start+k*step)%cells.size();if(!cells[i].empty())return i;}throw std::runtime_error("empty spatial grid");}
};
static std::vector<Op> spatial_trace(const Graph&g,const std::vector<Coordinate>&c,const std::string&kind,std::uint64_t seed,std::size_t n){
 if(c.size()!=g.vertex_count()||c.empty())throw std::runtime_error("coordinate/graph size mismatch");SpatialGrid grid(c);std::mt19937_64 r(seed);std::vector<Op>x;x.reserve(n);
 auto pick=[&](std::size_t cell){auto&v=grid.cells[cell];return v[r()%v.size()];};
 std::size_t cluster=grid.nonempty(seed%grid.cells.size()),moving=grid.nonempty(0);
 for(std::size_t i=0;i<n;i++){
  if(g.edge_count()&&r()%4==0){x.push_back({true,(std::uint32_t)(r()%g.edge_count()),0,r()%100});continue;}
  std::uint32_t s=0,t=0;
  if(kind=="local"){auto cell=grid.nonempty(r()%grid.cells.size());s=pick(cell);t=pick(cell);}
  else if(kind=="cross"){auto a=grid.nonempty(r()%grid.cells.size());auto ax=a%grid.side,ay=a/grid.side;auto opposite=(grid.side-1-ay)*grid.side+(grid.side-1-ax);auto b=grid.nonempty(opposite);s=pick(a);t=pick(b);}
  else if(kind=="clustered"){s=pick(cluster);t=pick(cluster);}
  else if(kind=="moving"){moving=grid.nonempty((moving+1)%grid.cells.size());s=pick(moving);t=pick(moving);}
  else throw std::runtime_error("unknown workload");x.push_back({false,s,t,0});
 }return x;
}
static std::uint64_t trace_hash(const std::vector<Op>&ops){std::uint64_t h=1469598103934665603ULL;auto mix=[&](std::uint64_t v){for(int i=0;i<8;i++){h^=(v>>(i*8))&255;h*=1099511628211ULL;}};for(auto&o:ops){mix(o.update);mix(o.a);mix(o.b);mix(o.w);}return h;}
static std::vector<Distance> oracle(Graph g,const std::vector<Op>&ops){std::vector<Distance> out;for(auto&o:ops)if(o.update)g.update_weight(o.a,o.w);else out.push_back(dijkstra(g,o.a).dist[o.b]);return out;}
template<class E>static std::uint64_t run_engine(const char*name,E&e,const std::vector<Op>&ops,const std::vector<Distance>&ref){
 std::size_t qi=0;auto t=Clock::now();for(auto&o:ops)if(o.update)e.update(o.a,o.w);else if(e.query(o.a,o.b)!=ref.at(qi++)){std::cerr<<name<<" exactness failure at query "<<qi-1<<"\n";std::exit(3);}
 return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();
}
struct FreshDijkstra{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return dijkstra(g,s).dist[t];}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
struct FreshBidir{Graph g;Distance query(std::uint32_t s,std::uint32_t t){return bidirectional_dijkstra(g,s,t);}void update(std::uint32_t id,Weight w){g.update_weight(id,w);}};
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: ades_bench graph BASELINE [ops] [seed] [rep] [workload] [coords] [policy] [oracle_file]\nBASELINE may be ORACLE to generate a reusable trace+oracle.\nworkload: mixed|local|cross|clustered|moving; policy: work|vertex|fixed\n";return 2;}
 std::string baseline=argv[2],workload=argc>6?argv[6]:"mixed",policy="work";
 if(workload=="mixed"){if(argc>7)policy=argv[7];}else if(argc>8)policy=argv[8];
 auto base=Graph::load_dimacs_gr_gz(argv[1]);
 std::size_t n=argc>3?std::strtoull(argv[3],nullptr,10):1000;std::uint64_t seed=argc>4?std::strtoull(argv[4],nullptr,10):7;int rep=argc>5?std::atoi(argv[5]):0;
 std::vector<Op> ops;if(workload=="mixed")ops=mixed_trace(base,seed,n);else{if(argc<8){std::cerr<<"spatial workload requires coordinate file\n";return 2;}auto coords=load_dimacs_co_gz(argv[7],base.vertex_count());ops=spatial_trace(base,coords,workload,seed,n);}
 std::string oracle_file;
 if(workload=="mixed"){if(argc>8)oracle_file=argv[8];}else if(argc>9)oracle_file=argv[9];
 std::vector<Distance> ref;
 auto generated_hash=trace_hash(ops);
 if(!oracle_file.empty()&&baseline!="ORACLE"){auto loaded=read_oracle(oracle_file);ops=std::move(loaded.first);ref=std::move(loaded.second);if(trace_hash(ops)!=generated_hash){std::cerr<<"oracle trace fingerprint mismatch\n";return 6;}}
 else ref=oracle(base,ops);
 auto hash=trace_hash(ops);
 if(baseline=="ORACLE"){if(oracle_file.empty()){std::cerr<<"ORACLE requires oracle_file\n";return 2;}write_oracle(oracle_file,ops,ref);std::cout<<"ORACLE,0,"<<seed<<","<<workload<<",work,"<<hash<<","<<ops.size()<<","<<ref.size()<<",0,0,0,0,0,0,0,0,0\n";return 0;}
 std::uint64_t ns=0;Stats stats{};
 if(baseline=="B0"){FreshDijkstra e{base};ns=run_engine("B0",e,ops,ref);}else if(baseline=="B1"){FreshBidir e{base};ns=run_engine("B1",e,ops,ref);}
 else if(baseline=="B2"){AlwaysResident e(base,ResidentMode::FullRebuild);ns=run_engine("B2",e,ops,ref);}else if(baseline=="B3"){AlwaysResident e(base,ResidentMode::LocalRepair);ns=run_engine("B3",e,ops,ref);}
 else if(baseline=="B4"){Config cfg;cfg.resident_cap=8;
  if(policy=="fixed")cfg.repair_policy=RepairPolicy::Fixed;else if(policy=="vertex")cfg.repair_policy=RepairPolicy::VertexOnly;
  else if(policy=="work")cfg.repair_policy=RepairPolicy::WorkAware;else{std::cerr<<"unknown policy "<<policy<<"\n";return 2;}
  ADES e(base,cfg);ns=run_engine("B4",e,ops,ref);stats=e.stats();}else{std::cerr<<"unknown baseline "<<baseline<<"\n";return 2;}
 std::cout<<baseline<<","<<rep<<","<<seed<<","<<workload<<","<<policy<<","<<hash<<","<<n<<","<<ref.size()<<","<<ns<<","<<stats.cold_queries<<","<<stats.resident_queries<<","<<stats.promotions<<","<<stats.rebuilds<<","<<stats.filtered_updates<<","<<stats.decrease_repairs<<","<<stats.increase_repairs<<","<<stats.repair_aborts<<"\n";
}
