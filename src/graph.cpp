#include "ades/graph.hpp"
#include <cstdio>
#include <stdexcept>
#include <zlib.h>
namespace ades {
std::uint32_t Graph::add_edge(std::uint32_t u,std::uint32_t v,Weight w){
 if(u>=vertex_count()||v>=vertex_count()) throw std::out_of_range("vertex");
 auto id=static_cast<std::uint32_t>(edges_.size()); edges_.push_back({u,v,w});
 out_[u].push_back({v,id}); in_[v].push_back({u,id}); return id;
}
Graph Graph::load_dimacs_gr_gz(const std::string& path){
 gzFile f=gzopen(path.c_str(),"rb"); if(!f) throw std::runtime_error("cannot open DIMACS gzip");
 char buf[4096]; std::size_t n=0; Graph g;
 while(gzgets(f,buf,sizeof(buf))){
   if(buf[0]=='p'){ unsigned long long nn,mm; if(std::sscanf(buf,"p sp %llu %llu",&nn,&mm)==2){n=nn;g=Graph(n);} }
   else if(buf[0]=='a'){ unsigned u,v; unsigned long long w; if(std::sscanf(buf,"a %u %u %llu",&u,&v,&w)==3) g.add_edge(u-1,v-1,w); }
 }
 gzclose(f); if(!n) throw std::runtime_error("missing DIMACS problem line"); return g;
}
}