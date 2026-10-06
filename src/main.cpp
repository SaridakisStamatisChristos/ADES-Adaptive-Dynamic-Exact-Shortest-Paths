#include "ades/ades.hpp"
#include <iostream>
int main(int argc,char**argv){
 if(argc<2){std::cerr<<"usage: ades_cli GRAPH.gr.gz [source target]\n";return 2;}
 auto g=ades::Graph::load_dimacs_gr_gz(argv[1]);
 std::cout<<"nodes="<<g.vertex_count()<<" edges="<<g.edge_count()<<"\n";
 if(argc>=4){auto s=std::stoul(argv[2]),t=std::stoul(argv[3]);std::cout<<ades::bidirectional_dijkstra(g,s-1,t-1)<<"\n";}
}