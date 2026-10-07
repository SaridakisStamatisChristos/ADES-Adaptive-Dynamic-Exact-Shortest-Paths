#include "ades/coordinates.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <zlib.h>
namespace ades {
std::vector<Coordinate> load_dimacs_co_gz(const std::string& path,std::size_t expected){
 gzFile f=gzopen(path.c_str(),"rb");if(!f)throw std::runtime_error("cannot open DIMACS coordinate gzip");
 char buf[4096];std::vector<Coordinate> out(expected);std::vector<unsigned char> seen(expected,0);std::size_t count=0,max_id=0;
 while(gzgets(f,buf,sizeof(buf))){
  if(buf[0]!='v')continue;unsigned id;long long x,y;
  if(std::sscanf(buf,"v %u %lld %lld",&id,&x,&y)!=3||id==0)throw std::runtime_error("malformed DIMACS coordinate");
  if(x<std::numeric_limits<std::int32_t>::min()||x>std::numeric_limits<std::int32_t>::max()||y<std::numeric_limits<std::int32_t>::min()||y>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("DIMACS coordinate out of int32 range");
  max_id=max_id<id?id:max_id;
  if(out.size()<id){out.resize(id);seen.resize(id,0);}if(seen[id-1])throw std::runtime_error("duplicate DIMACS coordinate id");
  out[id-1]={(std::int32_t)x,(std::int32_t)y};seen[id-1]=1;count++;
 }
 gzclose(f);
 if(expected&&out.size()!=expected)throw std::runtime_error("coordinate vertex-count mismatch");
 if(expected&&count!=expected)throw std::runtime_error("missing DIMACS coordinates");
 if(!expected)out.resize(max_id);
 return out;
}
}
