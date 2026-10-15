#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using velographx::BasicIncrementalWeightedSSSP;
using velographx::VertexId;
using velographx::WeightedDynamicGraph;
using velographx::WeightedUpdateBatch;

struct Edge { VertexId u, v; std::uint64_t w; };

static std::vector<std::uint64_t> bellman(std::size_t n, VertexId source,
                                          const std::vector<Edge>& edges) {
  constexpr auto INF = BasicIncrementalWeightedSSSP<WeightedDynamicGraph>::kInf;
  std::vector<std::uint64_t> d(n, INF);
  d[source] = 0;
  for (std::size_t pass = 0; pass + 1 < n; ++pass) {
    bool changed = false;
    for (const auto& e : edges) {
      if (d[e.u] == INF || e.w > INF - d[e.u]) continue;
      const auto cand = d[e.u] + e.w;
      if (cand < d[e.v]) { d[e.v] = cand; changed = true; }
    }
    if (!changed) break;
  }
  return d;
}

static void set_edge(std::vector<Edge>& edges, VertexId u, VertexId v,
                     std::uint64_t w) {
  for (auto& e : edges) if (e.u == u && e.v == v) { e.w = w; return; }
  edges.push_back({u, v, w});
}

static void remove_edge(std::vector<Edge>& edges, VertexId u, VertexId v) {
  edges.erase(std::remove_if(edges.begin(), edges.end(),
                             [&](const Edge& e){ return e.u == u && e.v == v; }),
              edges.end());
}

static void init_graph(WeightedDynamicGraph& g, const std::vector<Edge>& edges) {
  WeightedUpdateBatch b;
  for (const auto& e : edges) b.add(e.u, e.v, e.w);
  g.apply(b);
}

static bool verify(const char* name, int step, const WeightedDynamicGraph& g,
                   const BasicIncrementalWeightedSSSP<WeightedDynamicGraph>& sssp,
                   VertexId source, const std::vector<Edge>& model) {
  const auto expected = bellman(g.vertex_count(), source, model);
  const auto& got = sssp.distances();
  if (got != expected) {
    for (std::size_t v = 0; v < expected.size(); ++v) {
      if (v >= got.size() || got[v] != expected[v]) {
        std::cerr << "MISMATCH case=" << name << " step=" << step
                  << " vertex=" << v << " got="
                  << (v < got.size() ? std::to_string(got[v]) : "MISSING")
                  << " expected=" << expected[v] << '\n';
        break;
      }
    }
    return false;
  }
  return true;
}

static bool simple_case() {
  std::vector<Edge> m{{0,1,1},{1,2,1},{0,2,5},{2,3,2},{1,3,10}};
  WeightedDynamicGraph g(4, true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,0);
  if (!verify("simple",0,g,s,0,m)) return false;
  WeightedUpdateBatch b1; b1.update(1,2,10); s.apply(b1); set_edge(m,1,2,10);
  if (!verify("simple",1,g,s,0,m)) return false;
  WeightedUpdateBatch b2; b2.update(1,2,0); s.apply(b2); set_edge(m,1,2,0);
  return verify("simple",2,g,s,0,m);
}

static bool zero_equal_case() {
  std::vector<Edge> m{{0,1,0},{0,2,0},{1,3,1},{2,3,1},{3,4,0}};
  WeightedDynamicGraph g(5,true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,0);
  if (!verify("zero_equal",0,g,s,0,m)) return false;
  WeightedUpdateBatch b; b.update(1,3,2); s.apply(b); set_edge(m,1,3,2);
  if (!verify("zero_equal",1,g,s,0,m)) return false;
  WeightedUpdateBatch c; c.update(2,3,3); s.apply(c); set_edge(m,2,3,3);
  return verify("zero_equal",2,g,s,0,m);
}

static bool asymmetric_case() {
  std::vector<Edge> m{{0,1,2},{1,0,50},{1,2,3},{0,2,20},{2,3,4}};
  WeightedDynamicGraph g(4,true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,0);
  if (!verify("asymmetric",0,g,s,0,m)) return false;
  WeightedUpdateBatch b; b.update(1,0,1); s.apply(b); set_edge(m,1,0,1);
  if (!verify("asymmetric",1,g,s,0,m)) return false;
  WeightedUpdateBatch c; c.update(1,2,30); s.apply(c); set_edge(m,1,2,30);
  return verify("asymmetric",2,g,s,0,m);
}

static bool nonzero_source_case() {
  std::vector<Edge> m{{3,4,2},{4,5,3},{3,5,10},{0,1,1}};
  WeightedDynamicGraph g(6,true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,3);
  return verify("nonzero_source",0,g,s,3,m);
}

static bool removal_case() {
  std::vector<Edge> m{{0,1,1},{1,2,1},{0,2,8},{2,3,2}};
  WeightedDynamicGraph g(4,true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,0);
  WeightedUpdateBatch b; b.remove(1,2); s.apply(b); remove_edge(m,1,2);
  return verify("removal",1,g,s,0,m);
}

static bool random_case() {
  std::vector<Edge> m;
  for (VertexId i=0;i<15;++i) m.push_back({i,i+1,1+(i%5)});
  m.push_back({0,5,8}); m.push_back({2,9,12}); m.push_back({3,10,4});
  m.push_back({7,4,5}); m.push_back({10,6,3});
  WeightedDynamicGraph g(16,true); init_graph(g,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s(g,0);
  std::mt19937_64 rng(0xC0FFEEULL);
  for (int step=1; step<=200; ++step) {
    const std::size_t idx = static_cast<std::size_t>(rng()%m.size());
    const VertexId u=m[idx].u;
    const VertexId v=m[idx].v;
    const std::uint64_t w=static_cast<std::uint64_t>(rng()%31);
    WeightedUpdateBatch b; b.update(u,v,w); s.apply(b); set_edge(m,u,v,w);
    if (!verify("random_mixed",step,g,s,0,m)) return false;
  }
  return true;
}

static bool independent_sources_case() {
  std::vector<Edge> m{{0,1,2},{1,2,3},{2,3,4},{3,4,5},{4,0,6},{1,4,1}};
  WeightedDynamicGraph g0(5,true), g3(5,true); init_graph(g0,m); init_graph(g3,m);
  BasicIncrementalWeightedSSSP<WeightedDynamicGraph> s0(g0,0), s3(g3,3);
  WeightedUpdateBatch b; b.update(1,4,9);
  s0.apply(b); s3.apply(b); set_edge(m,1,4,9);
  return verify("independent_sources",1,g0,s0,0,m) &&
         verify("independent_sources",1,g3,s3,3,m);
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string c=argv[1];
  bool ok=false;
  if(c=="simple") ok=simple_case();
  else if(c=="zero_equal") ok=zero_equal_case();
  else if(c=="asymmetric") ok=asymmetric_case();
  else if(c=="nonzero_source") ok=nonzero_source_case();
  else if(c=="removal") ok=removal_case();
  else if(c=="random_mixed") ok=random_case();
  else if(c=="independent_sources") ok=independent_sources_case();
  else return 2;
  std::cout << (ok?"PASS":"FAIL") << " case=" << c << '\n';
  return ok?0:3;
}
