#include <boost/version.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

static_assert(BOOST_VERSION == 109200, "PR59 requires Boost 1.92.0");

using Dist = std::uint64_t;
static constexpr Dist INF = std::numeric_limits<Dist>::max() / 4;
using EdgeProp = boost::property<boost::edge_weight_t, Dist>;
using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                                    boost::no_property, EdgeProp>;

struct ModelEdge { std::uint32_t u, v; Dist w; };
struct Mismatch { std::uint32_t node; Dist boost_dist; Dist oracle_dist; };
struct CaseResult {
  std::string name;
  bool pass = true;
  std::size_t updates = 0;
  std::size_t increases = 0;
  std::size_t decreases = 0;
  std::size_t queries = 0;
  std::vector<Mismatch> mismatches;
};

static Graph build_graph(std::size_t n, const std::vector<ModelEdge>& edges) {
  Graph g(n);
  auto weights = boost::get(boost::edge_weight, g);
  for (const auto& e : edges) {
    auto [ed, inserted] = boost::add_edge(e.u, e.v, g);
    if (!inserted) throw std::runtime_error("edge insertion failed");
    weights[ed] = e.w;
  }
  return g;
}

static std::vector<Dist> boost_dijkstra(const Graph& g, std::uint32_t source) {
  std::vector<Dist> d(boost::num_vertices(g), INF);
  auto index = boost::get(boost::vertex_index, g);
  auto dm = boost::make_iterator_property_map(d.begin(), index);
  boost::dijkstra_shortest_paths(
      g, source,
      boost::distance_map(dm)
          .weight_map(boost::get(boost::edge_weight, g))
          .distance_inf(INF)
          .distance_zero(Dist{0}));
  return d;
}

static std::vector<Dist> oracle_bellman_ford(std::size_t n,
                                             const std::vector<ModelEdge>& edges,
                                             std::uint32_t source) {
  std::vector<Dist> d(n, INF);
  d[source] = 0;
  for (std::size_t i = 1; i < n; ++i) {
    bool changed = false;
    for (const auto& e : edges) {
      if (d[e.u] == INF || e.w > INF - d[e.u]) continue;
      const Dist cand = d[e.u] + e.w;
      if (cand < d[e.v]) {
        d[e.v] = cand;
        changed = true;
      }
    }
    if (!changed) break;
  }
  return d;
}

static void check_source(CaseResult& r, const Graph& g,
                         const std::vector<ModelEdge>& edges,
                         std::uint32_t source) {
  const auto got = boost_dijkstra(g, source);
  const auto want = oracle_bellman_ford(boost::num_vertices(g), edges, source);
  ++r.queries;
  for (std::size_t i = 0; i < got.size(); ++i) {
    if (got[i] != want[i]) {
      r.pass = false;
      if (r.mismatches.size() < 8)
        r.mismatches.push_back({static_cast<std::uint32_t>(i), got[i], want[i]});
    }
  }
}

static void update_weight(Graph& g, std::vector<ModelEdge>& edges,
                          std::uint32_t u, std::uint32_t v, Dist nw,
                          CaseResult& r) {
  auto it = std::find_if(edges.begin(), edges.end(), [&](const ModelEdge& e) {
    return e.u == u && e.v == v;
  });
  if (it == edges.end()) throw std::runtime_error("model edge missing");
  const Dist old = it->w;
  if (nw > old) ++r.increases;
  if (nw < old) ++r.decreases;
  it->w = nw;
  auto [ed, found] = boost::edge(u, v, g);
  if (!found) throw std::runtime_error("boost edge missing");
  boost::put(boost::edge_weight, g, ed, nw);
  ++r.updates;
}

static CaseResult run_case(std::string name, std::size_t n,
                           std::vector<ModelEdge> edges,
                           const std::vector<std::tuple<std::uint32_t,std::uint32_t,Dist>>& updates,
                           const std::vector<std::uint32_t>& sources) {
  CaseResult r;
  r.name = std::move(name);
  Graph g = build_graph(n, edges);
  for (auto s : sources) check_source(r, g, edges, s);
  for (const auto& update : updates) {
    auto [u,v,w] = update;
    update_weight(g, edges, u, v, w, r);
    for (auto s : sources) check_source(r, g, edges, s);
  }
  return r;
}

static CaseResult simple_case() {
  std::vector<ModelEdge> e = {
    {0,1,1},{1,2,1},{0,2,5},{2,3,1},{1,3,10},{3,4,1},{0,4,50}
  };
  std::vector<std::tuple<std::uint32_t,std::uint32_t,Dist>> u = {
    {1,2,10},{0,2,1},{2,3,20},{1,3,1}
  };
  return run_case("simple-directed-increase-decrease", 5, e, u, {0,1,2});
}

static CaseResult zero_case() {
  std::vector<ModelEdge> e = {
    {0,1,0},{0,2,0},{1,3,0},{2,3,0},{3,4,1},{1,5,2},{2,5,2},{5,6,1}
  };
  std::vector<std::tuple<std::uint32_t,std::uint32_t,Dist>> u = {
    {1,3,1},{2,3,2},{1,3,0},{0,2,3}
  };
  return run_case("zero-weight-equal-distance", 7, e, u, {0,1,2,3});
}

static CaseResult asymmetric_case() {
  std::vector<ModelEdge> e = {
    {0,1,1},{1,0,30},{1,2,1},{2,1,40},{0,3,8},{3,2,1},
    {2,4,1},{4,2,50},{4,5,1},{1,5,20}
  };
  std::vector<std::tuple<std::uint32_t,std::uint32_t,Dist>> u = {
    {1,2,15},{0,3,2},{3,2,20},{1,5,2}
  };
  return run_case("asymmetric-directed-arcs", 6, e, u, {0,2,5});
}

static CaseResult random_case() {
  constexpr std::size_t n = 32;
  std::mt19937_64 rng(20261015ULL);
  std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
  std::vector<ModelEdge> e;
  for (std::uint32_t u = 0; u < n; ++u) {
    const std::uint32_t v = (u + 1) % n;
    const Dist w = 1 + (rng() % 5);
    e.push_back({u,v,w});
    seen.insert({u,v});
  }
  while (e.size() < 180) {
    const std::uint32_t u = rng() % n, v = rng() % n;
    if (u == v || seen.count({u,v})) continue;
    const Dist w = rng() % 16;
    e.push_back({u,v,w});
    seen.insert({u,v});
  }

  CaseResult r;
  r.name = "random-directed-mixed";
  Graph g = build_graph(n, e);
  for (auto s : {0u, 7u, 19u, 31u}) check_source(r, g, e, s);

  for (std::size_t step = 0; step < 120; ++step) {
    const std::size_t idx = rng() % e.size();
    const auto u = e[idx].u, v = e[idx].v;
    const Dist old = e[idx].w;
    Dist nw;
    if ((step % 2 == 0) || old == 0) {
      nw = old + 1 + (rng() % 10);
    } else {
      const Dist delta = 1 + (rng() % std::min<Dist>(old, 8));
      nw = old - delta;
    }
    update_weight(g, e, u, v, nw, r);
    check_source(r, g, e, 0);
    check_source(r, g, e, static_cast<std::uint32_t>(step % n));
    check_source(r, g, e, static_cast<std::uint32_t>((7 * step + 3) % n));
  }
  return r;
}

static std::string jescape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '\\' || c == '"') o.push_back('\\');
    o.push_back(c);
  }
  return o;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: probe_boost_graph_dijkstra REPORT.json\n";
    return 2;
  }

  std::vector<CaseResult> cases;
  try {
    cases.push_back(simple_case());
    cases.push_back(zero_case());
    cases.push_back(asymmetric_case());
    cases.push_back(random_case());
  } catch (const std::exception& e) {
    std::cerr << "probe exception: " << e.what() << "\n";
    return 3;
  }

  bool pass = true;
  std::size_t inc = 0, dec = 0, updates = 0, queries = 0;
  for (const auto& c : cases) {
    pass = pass && c.pass;
    inc += c.increases;
    dec += c.decreases;
    updates += c.updates;
    queries += c.queries;
    std::cout << "[boost-preflight] " << c.name << ": "
              << (c.pass ? "PASS" : "FAIL")
              << " updates=" << c.updates << " queries=" << c.queries << "\n";
  }

  std::ofstream out(argv[1]);
  out << "{\n";
  out << "  \"schema\": \"ades-boost-graph-dijkstra-preflight-v1\",\n";
  out << "  \"boost_version\": \"" << BOOST_LIB_VERSION << "\",\n";
  out << "  \"boost_graph_commit\": \"fc5dac2c552f945846449fd123a9b8335d5c063a\",\n";
  out << "  \"boost_release_sha256\": \"5c1d40cb8e19adbf740a4ec2da35b3e58f3f5804b1dce44deb53df72193cbc6c\",\n";
  out << "  \"directed\": true,\n";
  out << "  \"nonnegative_weights\": true,\n";
  out << "  \"updates\": " << updates << ",\n";
  out << "  \"increase_updates\": " << inc << ",\n";
  out << "  \"decrease_updates\": " << dec << ",\n";
  out << "  \"oracle_queries\": " << queries << ",\n";
  out << "  \"semantic_preflight_pass\": " << (pass ? "true" : "false") << ",\n";
  out << "  \"direct_status_after_this_probe\": \"PENDING\",\n";
  out << "  \"remaining_direct_gates\": [\"matched_trace_adapter\", \"persistent_memory_budget_enforceable\", \"persistent_memory_accounting_auditable\", \"timing_boundary_declared\"],\n";
  out << "  \"cases\": [\n";
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const auto& c = cases[i];
    out << "    {\"name\": \"" << jescape(c.name)
        << "\", \"pass\": " << (c.pass ? "true" : "false")
        << ", \"updates\": " << c.updates
        << ", \"increases\": " << c.increases
        << ", \"decreases\": " << c.decreases
        << ", \"queries\": " << c.queries
        << ", \"mismatch_count\": " << c.mismatches.size() << "}";
    if (i + 1 != cases.size()) out << ',';
    out << "\n";
  }
  out << "  ]\n}\n";
  return pass ? 0 : 1;
}
