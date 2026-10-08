#include "ades/ades.hpp"
#include "ades/baselines.hpp"
#include "ades/coordinates.hpp"
#include "ades/trace.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ades;
using Clock = std::chrono::steady_clock;

namespace {

void write_oracle(const std::string& path, const std::vector<TraceOp>& ops,
                  const std::vector<Distance>& ref) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write oracle: " + path);

  const auto counts = trace_counts(ops);
  if (counts.query_count != ref.size()) {
    throw std::runtime_error("oracle answer count does not match trace query count");
  }

  out << "ADES_ORACLE_V2 " << ops.size() << ' ' << ref.size() << ' '
      << trace_sha256(ops) << '\n';
  std::size_t query = 0;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) {
      out << "UPDATE " << op.a << ' ' << op.old_weight << ' '
          << op.new_weight << '\n';
    } else {
      out << "QUERY " << op.a << ' ' << op.b << ' ' << ref.at(query++) << '\n';
    }
  }
  if (!out) throw std::runtime_error("failed while writing oracle: " + path);
}

std::pair<std::vector<TraceOp>, std::vector<Distance>> read_oracle(
    const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read oracle: " + path);

  std::string magic, expected_sha;
  std::size_t operation_count = 0, query_count = 0;
  if (!(in >> magic >> operation_count >> query_count >> expected_sha) ||
      magic != "ADES_ORACLE_V2") {
    throw std::runtime_error("bad oracle header");
  }

  std::vector<TraceOp> ops;
  std::vector<Distance> ref;
  ops.reserve(operation_count);
  ref.reserve(query_count);

  for (std::size_t i = 0; i < operation_count; ++i) {
    std::string kind;
    if (!(in >> kind)) throw std::runtime_error("truncated oracle");
    if (kind == "UPDATE") {
      std::uint32_t edge_id = 0;
      Weight old_weight = 0, new_weight = 0;
      if (!(in >> edge_id >> old_weight >> new_weight)) {
        throw std::runtime_error("bad oracle UPDATE");
      }
      ops.push_back(TraceOp::update(edge_id, old_weight, new_weight));
    } else if (kind == "QUERY") {
      std::uint32_t source = 0, target = 0;
      Distance answer = 0;
      if (!(in >> source >> target >> answer)) {
        throw std::runtime_error("bad oracle QUERY");
      }
      ops.push_back(TraceOp::query(source, target));
      ref.push_back(answer);
    } else {
      throw std::runtime_error("bad oracle operation");
    }
  }

  if (ref.size() != query_count) {
    throw std::runtime_error("oracle query-count mismatch");
  }
  if (trace_sha256(ops) != expected_sha) {
    throw std::runtime_error("oracle trace SHA-256 mismatch");
  }
  return {std::move(ops), std::move(ref)};
}

std::vector<TraceOp> mixed_trace(const Graph& graph, std::uint64_t seed,
                                 std::size_t operation_count) {
  Graph trace_graph = graph;
  std::mt19937_64 rng(seed);
  std::vector<TraceOp> ops;
  ops.reserve(operation_count);

  for (std::size_t i = 0; i < operation_count; ++i) {
    if (trace_graph.edge_count() && rng() % 4 == 0) {
      const auto edge_id =
          static_cast<std::uint32_t>(rng() % trace_graph.edge_count());
      const auto old_weight = trace_graph.edge(edge_id).weight;
      const Weight new_weight = rng() % 100;
      ops.push_back(TraceOp::update(edge_id, old_weight, new_weight));
      trace_graph.update_weight(edge_id, new_weight);
    } else {
      ops.push_back(TraceOp::query(
          static_cast<std::uint32_t>(rng() % trace_graph.vertex_count()),
          static_cast<std::uint32_t>(rng() % trace_graph.vertex_count())));
    }
  }
  return ops;
}

struct SpatialGrid {
  static constexpr std::size_t side = 32;
  std::vector<std::vector<std::uint32_t>> cells;
  std::int64_t minx, miny, dx, dy;

  explicit SpatialGrid(const std::vector<Coordinate>& coordinates)
      : cells(side * side) {
    auto [xmin, xmax] = std::minmax_element(
        coordinates.begin(), coordinates.end(),
        [](const auto& a, const auto& b) { return a.x < b.x; });
    auto [ymin, ymax] = std::minmax_element(
        coordinates.begin(), coordinates.end(),
        [](const auto& a, const auto& b) { return a.y < b.y; });
    minx = xmin->x;
    miny = ymin->y;
    dx = std::max<std::int64_t>(1, std::int64_t(xmax->x) - minx + 1);
    dy = std::max<std::int64_t>(1, std::int64_t(ymax->y) - miny + 1);
    for (std::uint32_t v = 0; v < coordinates.size(); ++v) {
      const auto ix = std::min<std::size_t>(
          side - 1,
          (std::uint64_t(std::int64_t(coordinates[v].x) - minx) * side) / dx);
      const auto iy = std::min<std::size_t>(
          side - 1,
          (std::uint64_t(std::int64_t(coordinates[v].y) - miny) * side) / dy);
      cells[iy * side + ix].push_back(v);
    }
  }

  std::size_t nonempty(std::size_t start, std::size_t step = 1) const {
    for (std::size_t k = 0; k < cells.size(); ++k) {
      const auto i = (start + k * step) % cells.size();
      if (!cells[i].empty()) return i;
    }
    throw std::runtime_error("empty spatial grid");
  }
};

std::vector<TraceOp> spatial_trace(const Graph& graph,
                                   const std::vector<Coordinate>& coordinates,
                                   const std::string& kind, std::uint64_t seed,
                                   std::size_t operation_count) {
  if (coordinates.size() != graph.vertex_count() || coordinates.empty()) {
    throw std::runtime_error("coordinate/graph size mismatch");
  }

  Graph trace_graph = graph;
  SpatialGrid grid(coordinates);
  std::mt19937_64 rng(seed);
  std::vector<TraceOp> ops;
  ops.reserve(operation_count);
  auto pick = [&](std::size_t cell) {
    const auto& vertices = grid.cells[cell];
    return vertices[rng() % vertices.size()];
  };

  std::size_t cluster = grid.nonempty(seed % grid.cells.size());
  std::size_t moving = grid.nonempty(0);
  for (std::size_t i = 0; i < operation_count; ++i) {
    if (trace_graph.edge_count() && rng() % 4 == 0) {
      const auto edge_id =
          static_cast<std::uint32_t>(rng() % trace_graph.edge_count());
      const auto old_weight = trace_graph.edge(edge_id).weight;
      const Weight new_weight = rng() % 100;
      ops.push_back(TraceOp::update(edge_id, old_weight, new_weight));
      trace_graph.update_weight(edge_id, new_weight);
      continue;
    }

    std::uint32_t source = 0, target = 0;
    if (kind == "local") {
      const auto cell = grid.nonempty(rng() % grid.cells.size());
      source = pick(cell);
      target = pick(cell);
    } else if (kind == "cross") {
      const auto a = grid.nonempty(rng() % grid.cells.size());
      const auto ax = a % grid.side, ay = a / grid.side;
      const auto opposite =
          (grid.side - 1 - ay) * grid.side + (grid.side - 1 - ax);
      const auto b = grid.nonempty(opposite);
      source = pick(a);
      target = pick(b);
    } else if (kind == "clustered") {
      source = pick(cluster);
      target = pick(cluster);
    } else if (kind == "moving") {
      moving = grid.nonempty((moving + 1) % grid.cells.size());
      source = pick(moving);
      target = pick(moving);
    } else {
      throw std::runtime_error("unknown workload");
    }
    ops.push_back(TraceOp::query(source, target));
  }
  return ops;
}

std::vector<Distance> oracle(Graph graph, const std::vector<TraceOp>& ops) {
  std::vector<Distance> out;
  out.reserve(trace_counts(ops).query_count);
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) {
      graph.update_weight(op.a, op.new_weight);
    } else {
      out.push_back(dijkstra(graph, op.a).dist.at(op.b));
    }
  }
  return out;
}

template <class Engine>
std::uint64_t run_engine(const char* name, Engine& engine,
                         const std::vector<TraceOp>& ops,
                         const std::vector<Distance>& ref) {
  std::size_t query = 0;
  const auto start = Clock::now();
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Update) {
      engine.update(op.a, op.new_weight);
    } else if (engine.query(op.a, op.b) != ref.at(query++)) {
      std::cerr << name << " exactness failure at query " << query - 1 << '\n';
      std::exit(3);
    }
  }
  if (query != ref.size()) {
    throw std::runtime_error("engine query count differs from oracle");
  }
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
          .count());
}

struct FreshDijkstra {
  Graph graph;
  Distance query(std::uint32_t source, std::uint32_t target) {
    return dijkstra(graph, source).dist.at(target);
  }
  void update(std::uint32_t edge_id, Weight weight) {
    graph.update_weight(edge_id, weight);
  }
};

struct FreshBidir {
  Graph graph;
  Distance query(std::uint32_t source, std::uint32_t target) {
    return bidirectional_dijkstra(graph, source, target);
  }
  void update(std::uint32_t edge_id, Weight weight) {
    graph.update_weight(edge_id, weight);
  }
};

int run(int argc, char** argv) {
  if (argc < 3) {
    std::cerr
        << "usage: ades_bench graph BASELINE [ops] [seed] [rep] [workload] "
           "[coords] [policy] [oracle_file]\n"
           "BASELINE may be ORACLE to generate a reusable trace+oracle.\n"
           "workload: mixed|local|cross|clustered|moving; "
           "policy: work|vertex|fixed\n"
           "Set ADES_TRACE_FILE=/path/to/trace to write/replay the canonical "
           "matched trace.\n";
    return 2;
  }

  const std::string baseline = argv[2];
  const std::string workload = argc > 6 ? argv[6] : "mixed";
  std::string policy = "work";
  if (workload == "mixed") {
    if (argc > 7) policy = argv[7];
  } else if (argc > 8) {
    policy = argv[8];
  }

  const auto base = Graph::load_dimacs_gr_gz(argv[1]);
  const std::size_t operation_count =
      argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1000;
  const std::uint64_t seed =
      argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 7;
  const int rep = argc > 5 ? std::atoi(argv[5]) : 0;

  std::vector<TraceOp> generated;
  if (workload == "mixed") {
    generated = mixed_trace(base, seed, operation_count);
  } else {
    if (argc < 8) {
      std::cerr << "spatial workload requires coordinate file\n";
      return 2;
    }
    const auto coordinates =
        load_dimacs_co_gz(argv[7], base.vertex_count());
    generated = spatial_trace(base, coordinates, workload, seed, operation_count);
  }
  validate_trace_against_graph(base, generated);
  const auto generated_sha = trace_sha256(generated);

  std::string oracle_file;
  if (workload == "mixed") {
    if (argc > 8) oracle_file = argv[8];
  } else if (argc > 9) {
    oracle_file = argv[9];
  }

  std::vector<TraceOp> ops = generated;
  const char* trace_file = std::getenv("ADES_TRACE_FILE");
  if (trace_file && *trace_file) {
    if (baseline == "ORACLE") {
      write_trace(trace_file, generated);
    } else {
      auto replay = read_trace(trace_file);
      validate_trace_against_graph(base, replay);
      const auto replay_sha = trace_sha256(replay);
      if (replay_sha != generated_sha) {
        std::cerr << "canonical replay trace SHA-256 mismatch: generated="
                  << generated_sha << " replay=" << replay_sha << '\n';
        return 6;
      }
      ops = std::move(replay);
    }
  }

  std::vector<Distance> ref;
  if (!oracle_file.empty() && baseline != "ORACLE") {
    auto loaded = read_oracle(oracle_file);
    validate_trace_against_graph(base, loaded.first);
    if (trace_sha256(loaded.first) != trace_sha256(ops)) {
      std::cerr << "oracle trace SHA-256 mismatch\n";
      return 6;
    }
    ref = std::move(loaded.second);
  } else {
    ref = oracle(base, ops);
  }

  const auto sha = trace_sha256(ops);
  const auto counts = trace_counts(ops);
  if (counts.query_count != ref.size()) {
    throw std::runtime_error("trace query count differs from oracle answer count");
  }

  if (baseline == "ORACLE") {
    if (oracle_file.empty()) {
      std::cerr << "ORACLE requires oracle_file\n";
      return 2;
    }
    write_oracle(oracle_file, ops, ref);
    std::cout << "ORACLE,0," << seed << ',' << workload << ',' << policy << ','
              << sha << ',' << ops.size() << ',' << counts.query_count << ','
              << counts.update_count << ',' << counts.increase_count << ','
              << counts.decrease_count
              << ",0,0,0,0,0,0,0,0,0\n";
    return 0;
  }

  std::uint64_t ns = 0;
  Stats stats{};
  if (baseline == "B0") {
    FreshDijkstra engine{base};
    ns = run_engine("B0", engine, ops, ref);
  } else if (baseline == "B1") {
    FreshBidir engine{base};
    ns = run_engine("B1", engine, ops, ref);
  } else if (baseline == "B2") {
    AlwaysResident engine(base, ResidentMode::FullRebuild);
    ns = run_engine("B2", engine, ops, ref);
  } else if (baseline == "B3") {
    AlwaysResident engine(base, ResidentMode::LocalRepair);
    ns = run_engine("B3", engine, ops, ref);
  } else if (baseline == "B4") {
    Config cfg;
    cfg.resident_cap = 8;
    if (policy == "fixed") {
      cfg.repair_policy = RepairPolicy::Fixed;
    } else if (policy == "vertex") {
      cfg.repair_policy = RepairPolicy::VertexOnly;
    } else if (policy == "work") {
      cfg.repair_policy = RepairPolicy::WorkAware;
    } else {
      std::cerr << "unknown policy " << policy << '\n';
      return 2;
    }
    ADES engine(base, cfg);
    ns = run_engine("B4", engine, ops, ref);
    stats = engine.stats();
  } else {
    std::cerr << "unknown baseline " << baseline << '\n';
    return 2;
  }

  std::cout << baseline << ',' << rep << ',' << seed << ',' << workload << ','
            << policy << ',' << sha << ',' << ops.size() << ','
            << counts.query_count << ',' << counts.update_count << ','
            << counts.increase_count << ',' << counts.decrease_count << ',' << ns
            << ',' << stats.cold_queries << ',' << stats.resident_queries << ','
            << stats.promotions << ',' << stats.rebuilds << ','
            << stats.filtered_updates << ',' << stats.decrease_repairs << ','
            << stats.increase_repairs << ',' << stats.repair_aborts << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "ades_bench: " << error.what() << '\n';
    return 2;
  }
}
