#include "ades/ades.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace ades;
struct Op{char kind;std::uint32_t a,b;std::uint64_t x;};
int main(int argc,char**argv){
 if(argc!=2){std::cerr<<"usage: ades_trace_replay TRACE\n";return 2;}
 std::ifstream in(argv[1]);if(!in){std::cerr<<"cannot open trace\n";return 2;}
 std::string line;std::size_t n=0;std::vector<Edge> initial;std::vector<Op> ops;
 while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;std::istringstream s(line);char k;s>>k;
  if(k=='n')s>>n;
  else if(k=='e'){Edge e{};s>>e.from>>e.to>>e.weight;initial.push_back(e);}
  else if(k=='u'){Op o{'u',0,0,0};s>>o.a>>o.x;ops.push_back(o);}
  else if(k=='q'){Op o{'q',0,0,0};s>>o.a>>o.b;ops.push_back(o);}
  else{std::cerr<<"bad trace line: "<<line<<"\n";return 2;}
 }
 Graph oracle(n),subject(n);for(auto&e:initial){oracle.add_edge(e.from,e.to,e.weight);subject.add_edge(e.from,e.to,e.weight);}
 Config c;c.resident_cap=4;c.probation_queries=1;c.promotion_ratio=0.0;c.repair_safety_ceiling={100000,500000};ADES a(std::move(subject),c);
 for(std::size_t i=0;i<ops.size();i++){auto&o=ops[i];
  if(o.kind=='u'){oracle.update_weight(o.a,o.x);a.update(o.a,o.x);}
  else{auto expected=dijkstra(oracle,o.a).dist[o.b],got=a.query(o.a,o.b);if(expected!=got){std::cerr<<"mismatch op="<<i<<" expected="<<expected<<" got="<<got<<"\n";return 1;}}
 }
 std::cout<<"trace exact: ops="<<ops.size()<<"\n";return 0;
}
