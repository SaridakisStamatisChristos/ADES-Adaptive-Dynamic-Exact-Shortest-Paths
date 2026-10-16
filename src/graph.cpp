#include "ades/graph.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <zlib.h>
namespace ades {
std::uint32_t Graph::add_edge(std::uint32_t u,std::uint32_t v,Weight w){
 if(frozen_)throw std::logic_error("cannot add edge after topology freeze");
 if(u>=vertex_count()||v>=vertex_count()) throw std::out_of_range("vertex");
 if(edges_.size()>=std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("32-bit edge id space exhausted");
 auto id=static_cast<std::uint32_t>(edges_.size()); edges_.push_back({u,v,w});
 out_[u].push_back({v,id}); in_[v].push_back({u,id}); return id;
}
void Graph::freeze_topology(){
 if(frozen_)return;
 out_flat_.clear();in_flat_.clear();out_offsets_.assign(vertex_count_+1,0);in_offsets_.assign(vertex_count_+1,0);
 out_flat_.reserve(edges_.size());in_flat_.reserve(edges_.size());
 for(std::size_t u=0;u<vertex_count_;++u){
  out_offsets_[u]=out_flat_.size();
  out_flat_.insert(out_flat_.end(),out_[u].begin(),out_[u].end());
  in_offsets_[u]=in_flat_.size();
  in_flat_.insert(in_flat_.end(),in_[u].begin(),in_[u].end());
 }
 out_offsets_[vertex_count_]=out_flat_.size();in_offsets_[vertex_count_]=in_flat_.size();
 std::vector<std::vector<Arc>>().swap(out_);std::vector<std::vector<Arc>>().swap(in_);
 frozen_=true;
}
Graph Graph::load_dimacs_gr_gz(const std::string& path){
 gzFile f=gzopen(path.c_str(),"rb"); if(!f) throw std::runtime_error("cannot open DIMACS gzip");
 char buf[4096]; std::size_t n=0; Graph g;
 while(gzgets(f,buf,sizeof(buf))){
   if(buf[0]=='p'){ unsigned long long nn,mm; if(std::sscanf(buf,"p sp %llu %llu",&nn,&mm)==2){n=nn;g=Graph(n);} }
   else if(buf[0]=='a'){ unsigned u,v; unsigned long long w; if(std::sscanf(buf,"a %u %u %llu",&u,&v,&w)==3) g.add_edge(u-1,v-1,w); }
 }
 gzclose(f); if(!n) throw std::runtime_error("missing DIMACS problem line"); g.freeze_topology();return g;
}
}