#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
#include "graph.h"
#include "heap.h"
#include "rr.h"
}

namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t OINF = std::numeric_limits<std::uint64_t>::max() / 4;

struct Edge { std::uint32_t u, v; int w; };
struct Group { std::uint32_t u, v; int min_w; std::vector<std::uint32_t> members; };
struct Op {
  bool query;
  std::uint32_t a, b;
  std::uint64_t old_w, new_w;
};
struct Model {
  std::uint32_t n{};
  std::vector<Edge> edges;
  std::vector<std::uint32_t> edge_group;
  std::vector<Group> groups;
  std::vector<std::vector<std::uint32_t>> outgoing;
};
struct RRState {
  vertex* graph = nullptr;
  heap* queue = nullptr;
  std::uint32_t source = std::numeric_limits<std::uint32_t>::max();
};

[[noreturn]] void fail(const std::string& s) { throw std::runtime_error(s); }
std::uint64_t ns(Clock::time_point s) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - s).count();
}
std::uint64_t key(std::uint32_t u, std::uint32_t v) {
  return (std::uint64_t(u) << 32) | v;
}
std::uint32_t perm(std::uint32_t x, std::uint32_t source) {
  if (x == source) return 0;
  if (x == 0) return source;
  return x;
}

Model load_graph(const std::string& path) {
  std::ifstream in(path);
  if (!in) fail("cannot open graph");
  Model m;
  std::string line;
  std::uint32_t declared_m = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == 'c') continue;
    if (line[0] == 'p') {
      std::string p, sp;
      std::istringstream ss(line);
      ss >> p >> sp >> m.n >> declared_m;
      if (!ss || p != "p" || sp != "sp" || !m.n) fail("bad problem line");
      m.edges.reserve(declared_m);
    } else if (line[0] == 'a') {
      char a;
      std::uint32_t u, v;
      std::uint64_t w;
      std::istringstream ss(line);
      ss >> a >> u >> v >> w;
      if (!ss || a != 'a' || u == 0 || v == 0 || u > m.n || v > m.n) fail("bad arc");
      if (w > std::uint64_t(INT_MAX - 500)) fail("edge weight outside RR int domain");
      m.edges.push_back({u - 1, v - 1, int(w)});
    }
  }
  if (m.edges.size() != declared_m) fail("DIMACS edge count mismatch");

  std::unordered_map<std::uint64_t, std::uint32_t> by_pair;
  by_pair.reserve(m.edges.size());
  m.edge_group.resize(m.edges.size());
  for (std::uint32_t i = 0; i < m.edges.size(); ++i) {
    const auto& e = m.edges[i];
    const auto k = key(e.u, e.v);
    auto it = by_pair.find(k);
    std::uint32_t gid;
    if (it == by_pair.end()) {
      gid = static_cast<std::uint32_t>(m.groups.size());
      by_pair.emplace(k, gid);
      m.groups.push_back({e.u, e.v, e.w, {}});
    } else gid = it->second;
    m.edge_group[i] = gid;
    auto& g = m.groups[gid];
    g.members.push_back(i);
    g.min_w = std::min(g.min_w, e.w);
  }
  m.outgoing.assign(m.n, {});
  for (std::uint32_t i = 0; i < m.edges.size(); ++i) m.outgoing[m.edges[i].u].push_back(i);
  return m;
}

std::vector<Op> load_trace(const std::string& path) {
  std::ifstream in(path);
  if (!in) fail("cannot open trace");
  std::vector<Op> ops;
  std::string line, kind;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    ss >> kind;
    if (kind == "QUERY") {
      std::uint32_t s, t;
      ss >> s >> t;
      if (!ss) fail("bad QUERY");
      ops.push_back({true, s, t, 0, 0});
    } else if (kind == "UPDATE") {
      std::uint32_t id;
      std::uint64_t old_w, new_w;
      ss >> id >> old_w >> new_w;
      if (!ss) fail("bad UPDATE");
      ops.push_back({false, id, 0, old_w, new_w});
    } else fail("unknown trace operation");
  }
  return ops;
}

void free_rr(RRState& s, std::uint32_t n) {
  if (s.graph) g_free_graph(&s.graph, int(n));
  if (s.queue) {
    std::free(s.queue->node_vector);
    std::free(s.queue);
    s.queue = nullptr;
  }
  s.source = std::numeric_limits<std::uint32_t>::max();
}

RRState build_rr(const Model& m, std::uint32_t source) {
  RRState s;
  s.source = source;
  s.graph = g_create_graph(int(m.n));
  if (!s.graph) fail("g_create_graph failed");
  for (const auto& g : m.groups) {
    const auto u = perm(g.u, source), v = perm(g.v, source);
    if (!g_insert_edge(s.graph, int(u), int(v), g.min_w)) fail("g_insert_edge failed");
  }
  s.queue = heap_new();
  if (!s.queue) fail("heap_new failed");
  auto* root = static_cast<vtx_node*>(s.graph[0].h_node.key);
  root->cost = 0;
  s.graph[0].pi = -2;
  if (heap_insert(&s.graph[0].h_node, s.queue) != 0) fail("root heap insertion failed");
  rr_recalculate_shortest_path(s.graph, s.queue);
  return s;
}

int recompute_min(const Model& m, std::uint32_t gid) {
  int value = INT_MAX;
  for (auto id : m.groups[gid].members) value = std::min(value, m.edges[id].w);
  if (value == INT_MAX) fail("empty group");
  return value;
}

void apply_update(Model& m, RRState& rr, std::uint32_t edge_id,
                  std::uint64_t old_w, std::uint64_t new_w) {
  if (edge_id >= m.edges.size()) fail("update edge id out of range");
  if (new_w > std::uint64_t(INT_MAX - 500)) fail("updated weight outside RR int domain");
  auto& e = m.edges[edge_id];
  if (std::uint64_t(e.w) != old_w) fail("trace old weight mismatch");
  const auto gid = m.edge_group[edge_id];
  auto& group = m.groups[gid];
  const int old_min = group.min_w;
  e.w = int(new_w);
  const int new_min = recompute_min(m, gid);
  if (new_min == old_min) return;
  group.min_w = new_min;
  if (!rr.graph) return;
  const auto u = perm(group.u, rr.source), v = perm(group.v, rr.source);
  rr_remove_edge(rr.graph, int(u), int(v), rr.queue);
  rr_add_edge(rr.graph, int(u), int(v), new_min, rr.queue);
}

std::vector<std::uint64_t> oracle(const Model& m, std::uint32_t source) {
  std::vector<std::uint64_t> d(m.n, OINF);
  using Item = std::pair<std::uint64_t, std::uint32_t>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
  d[source] = 0;
  q.push({0, source});
  while (!q.empty()) {
    auto [du, u] = q.top(); q.pop();
    if (du != d[u]) continue;
    for (auto id : m.outgoing[u]) {
      const auto& e = m.edges[id];
      const auto nd = du + std::uint64_t(e.w);
      if (nd < d[e.v]) {
        d[e.v] = nd;
        q.push({nd, e.v});
      }
    }
  }
  return d;
}

int rr_dist(const RRState& rr, std::uint32_t original_vertex) {
  const auto mapped = perm(original_vertex, rr.source);
  return static_cast<vtx_node*>(rr.graph[mapped].h_node.key)->cost;
}

void json_string(std::ostream& o, const std::string& s) {
  o << '"';
  for (char c : s) { if (c == '\\' || c == '"') o << '\\'; o << c; }
  o << '"';
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 4) {
      std::cerr << "usage: probe_rr_arbitrary_trace GRAPH.gr TRACE.txt REPORT.json\n";
      return 2;
    }
    const auto prep_start = Clock::now();
    Model model = load_graph(argv[1]);
    const auto ops = load_trace(argv[2]);
    const auto preprocessing_ns = ns(prep_start);

    RRState rr;
    std::uint64_t query_ns = 0, update_ns = 0;
    std::uint64_t comparisons = 0, mismatches = 0;
    std::uint32_t queries = 0, updates = 0, rebuilds = 0, source_reuses = 0;
    std::uint32_t increases = 0, decreases = 0;
    std::uint32_t first_bad_query = 0, first_bad_node = 0;
    int first_rr = 0;
    std::uint64_t first_oracle = 0;
    bool have_bad = false, path_domain_ok = true;

    for (const auto& op : ops) {
      if (!op.query) {
        auto start = Clock::now();
        if (op.new_w > op.old_w) ++increases;
        if (op.new_w < op.old_w) ++decreases;
        apply_update(model, rr, op.a, op.old_w, op.new_w);
        update_ns += ns(start);
        ++updates;
        continue;
      }

      auto start = Clock::now();
      if (!rr.graph || rr.source != op.a) {
        free_rr(rr, model.n);
        rr = build_rr(model, op.a);
        ++rebuilds;
      } else ++source_reuses;
      const int got_target = rr_dist(rr, op.b);
      query_ns += ns(start);

      const auto want = oracle(model, op.a);
      for (std::uint32_t v = 0; v < model.n; ++v) {
        ++comparisons;
        const int got = rr_dist(rr, v);
        bool same;
        if (want[v] == OINF) same = got >= INF;
        else if (want[v] >= std::uint64_t(INF)) { path_domain_ok = false; same = false; }
        else same = got == int(want[v]);
        if (!same) {
          if (!have_bad) {
            have_bad = true;
            first_bad_query = queries;
            first_bad_node = v;
            first_rr = got;
            first_oracle = want[v];
          }
          ++mismatches;
        }
      }
      if (want[op.b] != OINF && want[op.b] < std::uint64_t(INF) &&
          got_target != int(want[op.b])) {
        // Full-state comparison above already records the mismatch; this branch
        // only keeps the target lookup inside the timed online boundary explicit.
      }
      ++queries;
    }

    // Conservative PR43 semantic-state estimate for the single retained RR state.
    // Graph edge storage is excluded as common input.  Count the RR-owned per-vertex
    // SSSP/heap metadata plus the retained heap pointer vector.  Adapter grouping is
    // intentionally NOT promoted as budget-qualified by this preflight.
    const std::uint64_t rr_state_bytes =
        std::uint64_t(model.n) * (sizeof(vtx_node) + sizeof(heap_node) + sizeof(int)) +
        std::uint64_t(HEAP_SIZE) * sizeof(heap_node*) + sizeof(heap);

    std::ofstream out(argv[3]);
    if (!out) fail("cannot write report");
    out << "{\n";
    out << "  \"schema\": \"ades-rr-arbitrary-source-preflight-v1\",\n";
    out << "  \"rr_commit\": \"20aa865a52b67bd0ff402e2946e51541db0b5473\",\n";
    out << "  \"external_source_modified\": false,\n";
    out << "  \"source_adapter\": \"swap requested source with vertex 0; one retained RR state; rebuild on source change\",\n";
    out << "  \"future_knowledge_used\": false,\n";
    out << "  \"vertices\": " << model.n << ",\n";
    out << "  \"original_edges\": " << model.edges.size() << ",\n";
    out << "  \"aggregate_edges\": " << model.groups.size() << ",\n";
    out << "  \"operation_count\": " << ops.size() << ",\n";
    out << "  \"query_count\": " << queries << ",\n";
    out << "  \"update_count\": " << updates << ",\n";
    out << "  \"increase_count\": " << increases << ",\n";
    out << "  \"decrease_count\": " << decreases << ",\n";
    out << "  \"source_rebuild_count\": " << rebuilds << ",\n";
    out << "  \"same_source_reuse_count\": " << source_reuses << ",\n";
    out << "  \"full_state_distance_comparisons\": " << comparisons << ",\n";
    out << "  \"mismatch_count\": " << mismatches << ",\n";
    out << "  \"exact_full_state\": " << ((mismatches == 0 && path_domain_ok) ? "true" : "false") << ",\n";
    out << "  \"path_int_domain_compatible\": " << (path_domain_ok ? "true" : "false") << ",\n";
    out << "  \"preprocessing_ns\": " << preprocessing_ns << ",\n";
    out << "  \"query_ns_including_source_rebuilds\": " << query_ns << ",\n";
    out << "  \"update_ns\": " << update_ns << ",\n";
    out << "  \"algorithm_ns\": " << (query_ns + update_ns) << ",\n";
    out << "  \"rr_core_persistent_state_bytes_conservative\": " << rr_state_bytes << ",\n";
    out << "  \"persistent_budget_status\": \"NOT_YET_QUALIFIED_ADAPTER_STATE_PENDING\",\n";
    out << "  \"heap_capacity\": " << HEAP_SIZE << ",\n";
    out << "  \"direct_status_after_this_probe\": \"PENDING\"";
    if (have_bad) {
      out << ",\n  \"first_mismatch\": {\"query_index\": " << first_bad_query
          << ", \"node\": " << first_bad_node << ", \"rr\": " << first_rr
          << ", \"oracle\": " << first_oracle << "}";
    }
    out << "\n}\n";

    std::cerr << "[rr-arbitrary] vertices=" << model.n
              << " queries=" << queries << " updates=" << updates
              << " rebuilds=" << rebuilds << " comparisons=" << comparisons
              << " mismatches=" << mismatches << " exact="
              << ((mismatches == 0 && path_domain_ok) ? 1 : 0)
              << " rr_core_bytes=" << rr_state_bytes << "\n";
    free_rr(rr, model.n);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "probe_rr_arbitrary_trace: " << e.what() << '\n';
    return 2;
  }
}
