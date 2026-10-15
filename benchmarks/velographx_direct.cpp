#include "ades/graph.hpp"
#include "ades/trace.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;
using VGraph = velographx::WeightedDynamicGraph;
using VSSSP = velographx::BasicIncrementalWeightedSSSP<VGraph>;
using VBatch = velographx::WeightedUpdateBatch;

struct OracleBundle {
  std::string trace_sha256;
  std::vector<ades::Distance> answers;
};

struct EndpointGroup {
  std::uint32_t u = 0;
  std::uint32_t v = 0;
  std::vector<std::uint32_t> edge_ids;
  ades::Weight min_weight = 0;
};

struct AdapterGraph {
  std::vector<EndpointGroup> groups;
  std::vector<std::size_t> edge_to_group;
  std::size_t nonself_group_count = 0;
  std::size_t parallel_group_count = 0;
  std::size_t conflicting_parallel_group_count = 0;
};

struct CacheEntry {
  std::uint32_t source = 0;
  std::uint64_t last_use = 0;
  std::unique_ptr<VGraph> graph;
  std::unique_ptr<VSSSP> sssp;
};

static std::uint64_t ns_since(Clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
          .count());
}

static std::uint64_t pair_key(std::uint32_t u, std::uint32_t v) {
  return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
}

static std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
  if (b > std::numeric_limits<std::uint64_t>::max() - a)
    throw std::overflow_error("persistent-state accounting overflow");
  return a + b;
}

static std::uint64_t checked_mul(std::uint64_t a, std::uint64_t b) {
  if (a && b > std::numeric_limits<std::uint64_t>::max() / a)
    throw std::overflow_error("persistent-state accounting overflow");
  return a * b;
}

static std::uint64_t parse_budget() {
  const char* raw = std::getenv("ADES_PERSISTENT_STATE_BUDGET_BYTES");
  if (!raw || !*raw) throw std::runtime_error("VeloGraphX DIRECT adapter requires an explicit persistent-state budget");
  char* end = nullptr;
  const auto value = std::strtoull(raw, &end, 10);
  if (!end || *end != '\0' || value == 0)
    throw std::runtime_error("invalid persistent-state budget");
  return value;
}

static OracleBundle read_oracle_checked(const std::string& path,
                                        const std::vector<ades::TraceOp>& trace) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read oracle: " + path);

  std::string magic, expected_sha;
  std::size_t operation_count = 0, query_count = 0;
  if (!(in >> magic >> operation_count >> query_count >> expected_sha) ||
      magic != "ADES_ORACLE_V2") {
    throw std::runtime_error("bad oracle header");
  }
  if (operation_count != trace.size())
    throw std::runtime_error("oracle operation count differs from canonical trace");
  if (expected_sha != ades::trace_sha256(trace))
    throw std::runtime_error("oracle trace SHA-256 differs from canonical trace");

  std::vector<ades::Distance> answers;
  answers.reserve(query_count);
  std::size_t query_index = 0;
  for (std::size_t i = 0; i < operation_count; ++i) {
    std::string kind;
    if (!(in >> kind)) throw std::runtime_error("truncated oracle");
    const auto& op = trace.at(i);
    if (kind == "UPDATE") {
      std::uint32_t edge_id = 0;
      ades::Weight old_weight = 0, new_weight = 0;
      if (!(in >> edge_id >> old_weight >> new_weight) ||
          op.kind != ades::TraceOpKind::Update || op.a != edge_id ||
          op.old_weight != old_weight || op.new_weight != new_weight) {
        throw std::runtime_error("oracle UPDATE differs from canonical trace");
      }
    } else if (kind == "QUERY") {
      std::uint32_t source = 0, target = 0;
      ades::Distance answer = 0;
      if (!(in >> source >> target >> answer) ||
          op.kind != ades::TraceOpKind::Query || op.a != source || op.b != target) {
        throw std::runtime_error("oracle QUERY differs from canonical trace");
      }
      answers.push_back(answer);
      ++query_index;
    } else {
      throw std::runtime_error("bad oracle operation kind");
    }
  }
  if (query_index != query_count || answers.size() != query_count)
    throw std::runtime_error("oracle query count mismatch");
  return {expected_sha, std::move(answers)};
}

static AdapterGraph build_adapter_graph(const ades::Graph& graph) {
  AdapterGraph out;
  out.edge_to_group.resize(graph.edge_count());
  out.groups.reserve(graph.edge_count());
  std::unordered_map<std::uint64_t, std::size_t> index;
  index.reserve(graph.edge_count() * 2 + 1);

  for (std::uint32_t id = 0; id < graph.edge_count(); ++id) {
    const auto& e = graph.edge(id);
    const auto key = pair_key(e.from, e.to);
    auto [it, inserted] = index.emplace(key, out.groups.size());
    if (inserted) {
      EndpointGroup group;
      group.u = e.from;
      group.v = e.to;
      group.min_weight = e.weight;
      out.groups.push_back(std::move(group));
    }
    const auto group_index = it->second;
    out.groups[group_index].edge_ids.push_back(id);
    out.groups[group_index].min_weight =
        std::min(out.groups[group_index].min_weight, e.weight);
    out.edge_to_group[id] = group_index;
  }

  for (const auto& group : out.groups) {
    if (group.u != group.v) ++out.nonself_group_count;
    if (group.edge_ids.size() > 1) {
      ++out.parallel_group_count;
      const auto first_weight = graph.edge(group.edge_ids.front()).weight;
      bool conflicting = false;
      for (const auto id : group.edge_ids) {
        if (graph.edge(id).weight != first_weight) {
          conflicting = true;
          break;
        }
      }
      if (conflicting) ++out.conflicting_parallel_group_count;
    }
  }
  return out;
}

static ades::Weight recompute_group_min(const ades::Graph& graph,
                                        const EndpointGroup& group) {
  ades::Weight best = std::numeric_limits<ades::Weight>::max();
  for (const auto id : group.edge_ids) best = std::min(best, graph.edge(id).weight);
  return best;
}

static std::unique_ptr<VGraph> build_velo_graph(std::size_t vertex_count,
                                                const AdapterGraph& adapted) {
  auto graph = std::make_unique<VGraph>(vertex_count, true);
  VBatch batch;
  batch.updates.reserve(adapted.nonself_group_count);
  for (const auto& group : adapted.groups) {
    if (group.u == group.v) continue;  // nonnegative self-loops do not affect SSSP distances
    if (group.min_weight >= VSSSP::kInf)
      throw std::runtime_error("VeloGraphX finite-distance weight domain exceeded");
    batch.add(group.u, group.v, group.min_weight);
  }
  graph->apply(batch);
  return graph;
}

static std::uint64_t per_entry_accounted_bytes(std::size_t vertex_count,
                                               std::size_t simple_edge_count) {
  // PR43/PR54 logical accounting deliberately excludes allocator/container
  // overhead and common graph input. A retained VeloGraphX source instance owns
  // a private graph copy plus its persistent distance vector, so both semantic
  // payloads are charged. Each private simple edge is charged as (u,v,weight),
  // even though the upstream map stores u implicitly in its adjacency bucket.
  std::uint64_t bytes = checked_mul(static_cast<std::uint64_t>(simple_edge_count),
                                    2u * sizeof(std::uint32_t) + sizeof(std::uint64_t));
  bytes = checked_add(bytes,
                      checked_mul(static_cast<std::uint64_t>(vertex_count),
                                  sizeof(std::uint64_t)));
  bytes = checked_add(bytes, sizeof(std::uint32_t) + sizeof(std::uint64_t));
  return bytes;
}

static void write_json_string(std::ostream& out, const std::string& value) {
  out << '"';
  for (const char c : value) {
    if (c == '\\' || c == '"') out << '\\';
    out << c;
  }
  out << '"';
}

int main(int argc, char** argv) {
  try {
    if (argc != 5) {
      std::cerr << "usage: velographx_direct GRAPH TRACE ORACLE REPORT.json\n";
      return 2;
    }
    const std::string graph_path = argv[1];
    const std::string trace_path = argv[2];
    const std::string oracle_path = argv[3];
    const std::string report_path = argv[4];
    const auto budget = parse_budget();

    const auto load_start = Clock::now();
    ades::Graph base = ades::Graph::load_dimacs_gr_gz(graph_path);
    const auto graph_load_ns = ns_since(load_start);
    const auto graph_sha = ades::file_sha256(graph_path);

    const auto trace = ades::read_trace(trace_path);
    ades::validate_trace_against_graph(base, trace);
    const auto trace_sha = ades::trace_sha256(trace);
    const auto counts = ades::trace_counts(trace);
    const auto oracle = read_oracle_checked(oracle_path, trace);
    if (oracle.trace_sha256 != trace_sha || oracle.answers.size() != counts.query_count)
      throw std::runtime_error("oracle identity/count mismatch");

    const auto preprocessing_start = Clock::now();
    auto adapted = build_adapter_graph(base);
    const auto entry_bytes = per_entry_accounted_bytes(base.vertex_count(),
                                                       adapted.nonself_group_count);
    const auto preprocessing_ns = ns_since(preprocessing_start);

    std::vector<CacheEntry> cache;
    std::set<std::uint32_t> unique_sources;
    std::uint64_t logical_clock = 0;
    std::uint64_t current_bytes = 0, peak_bytes = 0;
    std::uint64_t query_ns = 0, update_ns = 0;
    std::uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0;
    std::uint64_t propagated_updates = 0, min_unchanged_updates = 0;
    std::size_t answer_index = 0;
    bool exact = true;
    std::size_t first_bad_query = std::numeric_limits<std::size_t>::max();
    ades::Distance first_got = 0, first_expected = 0;

    for (const auto& op : trace) {
      if (op.kind == ades::TraceOpKind::Update) {
        const auto start = Clock::now();
        const auto group_index = adapted.edge_to_group.at(op.a);
        auto& group = adapted.groups.at(group_index);
        const auto old_min = group.min_weight;
        base.update_weight(op.a, op.new_weight);
        const auto new_min = recompute_group_min(base, group);
        group.min_weight = new_min;

        if (group.u != group.v && new_min != old_min) {
          if (new_min >= VSSSP::kInf)
            throw std::runtime_error("VeloGraphX finite-distance weight domain exceeded after update");
          VBatch batch;
          batch.update(group.u, group.v, new_min);
          for (auto& entry : cache) entry.sssp->apply(batch);
          ++propagated_updates;
        } else {
          ++min_unchanged_updates;
        }
        update_ns += ns_since(start);
      } else {
        unique_sources.insert(op.a);
        ++logical_clock;
        const auto start = Clock::now();
        ades::Distance got = 0;
        auto it = std::find_if(cache.begin(), cache.end(),
                               [&](const CacheEntry& e) { return e.source == op.a; });
        if (it != cache.end()) {
          ++cache_hits;
          it->last_use = logical_clock;
          const auto& distances = it->sssp->distances();
          if (op.b >= distances.size()) throw std::runtime_error("query target out of VeloGraphX range");
          got = distances[op.b];
        } else {
          ++cache_misses;
          auto graph = build_velo_graph(base.vertex_count(), adapted);
          auto sssp = std::make_unique<VSSSP>(*graph, op.a);
          const auto& distances = sssp->distances();
          if (op.b >= distances.size()) throw std::runtime_error("query target out of VeloGraphX range");
          got = distances[op.b];

          if (entry_bytes <= budget) {
            while (!cache.empty() && current_bytes + entry_bytes > budget) {
              auto victim = std::min_element(cache.begin(), cache.end(),
                                             [](const CacheEntry& a, const CacheEntry& b) {
                                               return a.last_use < b.last_use;
                                             });
              cache.erase(victim);
              current_bytes -= entry_bytes;
              ++cache_evictions;
            }
            if (current_bytes + entry_bytes <= budget) {
              CacheEntry entry;
              entry.source = op.a;
              entry.last_use = logical_clock;
              entry.graph = std::move(graph);
              entry.sssp = std::move(sssp);
              cache.push_back(std::move(entry));
              current_bytes += entry_bytes;
              peak_bytes = std::max(peak_bytes, current_bytes);
            }
          }
        }
        query_ns += ns_since(start);

        const auto expected = oracle.answers.at(answer_index);
        if (got != expected && exact) {
          exact = false;
          first_bad_query = answer_index;
          first_got = got;
          first_expected = expected;
        }
        ++answer_index;
      }
    }

    if (answer_index != oracle.answers.size())
      throw std::runtime_error("executed query count differs from oracle");
    if (peak_bytes > budget)
      throw std::runtime_error("persistent-state byte budget exceeded");

    std::ofstream out(report_path, std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write report");
    out << "{\n";
    out << "  \"schema\": \"ades-velographx-direct-adapter-v1\",\n";
    out << "  \"velographx_commit\": \"2aa84ce81a180487239c661b65639e5100136075\",\n";
    out << "  \"graph_sha256\": "; write_json_string(out, graph_sha); out << ",\n";
    out << "  \"trace_sha256\": "; write_json_string(out, trace_sha); out << ",\n";
    out << "  \"operation_count\": " << trace.size() << ",\n";
    out << "  \"query_count\": " << counts.query_count << ",\n";
    out << "  \"update_count\": " << counts.update_count << ",\n";
    out << "  \"increase_count\": " << counts.increase_count << ",\n";
    out << "  \"decrease_count\": " << counts.decrease_count << ",\n";
    out << "  \"unique_query_sources\": " << unique_sources.size() << ",\n";
    out << "  \"exact_answers\": " << (exact ? "true" : "false") << ",\n";
    out << "  \"oracle_trace_identity\": true,\n";
    out << "  \"online_arrival_order\": true,\n";
    out << "  \"future_knowledge_used\": false,\n";
    out << "  \"graph_load_ns\": " << graph_load_ns << ",\n";
    out << "  \"preprocessing_ns\": " << preprocessing_ns << ",\n";
    out << "  \"query_ns\": " << query_ns << ",\n";
    out << "  \"update_ns\": " << update_ns << ",\n";
    out << "  \"algorithm_ns\": " << (query_ns + update_ns) << ",\n";
    out << "  \"timing_boundary\": \"query/update adapter work only; source-instance construction, LRU admission/eviction, parallel-edge minimum maintenance, and updates to all retained VeloGraphX instances are timed; oracle comparison, graph load, trace validation, and initial endpoint-group conversion excluded; conversion reported as preprocessing_ns\",\n";
    out << "  \"persistent_state_budget_bytes\": " << budget << ",\n";
    out << "  \"accounted_algorithm_state_bytes\": " << current_bytes << ",\n";
    out << "  \"peak_accounted_algorithm_state_bytes\": " << peak_bytes << ",\n";
    out << "  \"accounting_version\": 2,\n";
    out << "  \"per_retained_source_accounted_bytes\": " << entry_bytes << ",\n";
    out << "  \"accounting_boundary\": \"PR43/PR54 logical model: common input graph and one shared endpoint-group adapter representation excluded; every retained source-specific VeloGraphX private simple-graph payload (u,v,weight), distance vector, source id, and LRU timestamp charged; allocator/container overhead remains visible in RSS\",\n";
    out << "  \"parallel_edge_semantics\": \"For nonnegative shortest paths, parallel arcs with one endpoint pair are exactly equivalent to a single arc at the current minimum weight. The adapter preserves every original edge ID, recomputes that endpoint minimum after each online edge-ID weight update, and forwards a VeloGraphX endpoint update iff the minimum changes. Nonnegative self-loops are omitted because they cannot improve a shortest-path distance.\",\n";
    out << "  \"simple_endpoint_group_count\": " << adapted.nonself_group_count << ",\n";
    out << "  \"parallel_endpoint_group_count\": " << adapted.parallel_group_count << ",\n";
    out << "  \"conflicting_initial_parallel_weight_group_count\": " << adapted.conflicting_parallel_group_count << ",\n";
    out << "  \"cache_hits\": " << cache_hits << ",\n";
    out << "  \"cache_misses\": " << cache_misses << ",\n";
    out << "  \"cache_evictions\": " << cache_evictions << ",\n";
    out << "  \"retained_sources_final\": " << cache.size() << ",\n";
    out << "  \"endpoint_minimum_updates_propagated\": " << propagated_updates << ",\n";
    out << "  \"edge_id_updates_with_unchanged_endpoint_minimum\": " << min_unchanged_updates << ",\n";
    out << "  \"upstream_algorithm_modified\": false,\n";
    out << "  \"direct_status_after_this_run\": \"PENDING_QUALIFICATION\"";
    if (!exact) {
      out << ",\n  \"first_mismatch\": {\"query_index\": " << first_bad_query
          << ", \"got\": " << first_got << ", \"expected\": " << first_expected << "}";
    }
    out << "\n}\n";
    if (!out) throw std::runtime_error("failed while writing report");

    std::cout << "VELO_DIRECT trace=" << trace_sha
              << " ops=" << trace.size()
              << " queries=" << counts.query_count
              << " updates=" << counts.update_count
              << " sources=" << unique_sources.size()
              << " exact=" << (exact ? 1 : 0)
              << " algorithm_ns=" << (query_ns + update_ns)
              << " preprocessing_ns=" << preprocessing_ns
              << " current_bytes=" << current_bytes
              << " peak_bytes=" << peak_bytes
              << " entry_bytes=" << entry_bytes
              << " hits=" << cache_hits
              << " misses=" << cache_misses
              << " evictions=" << cache_evictions << '\n';
    return exact ? 0 : 3;
  } catch (const std::exception& error) {
    std::cerr << "velographx_direct: " << error.what() << '\n';
    return 2;
  }
}
