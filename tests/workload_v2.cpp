#include "ades/trace.hpp"
#include "ades/workload.hpp"

#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ades;

static void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <class F>
static void require_throws(F&& f, const char* message) {
  try {
    f();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(message);
}

static Graph make_graph() {
  Graph g(12);
  for (std::uint32_t i = 0; i < 12; ++i) {
    g.add_edge(i, (i + 1) % 12, 1000 + i * 17);
    g.add_edge(i, (i + 5) % 12, 1500 + i * 23);
  }
  return g;
}

static std::vector<TraceOp> updates(const WorkloadArtifact& artifact) {
  std::vector<TraceOp> result;
  for (const auto& op : artifact.operations)
    if (op.kind == TraceOpKind::Update) result.push_back(op);
  return result;
}

static std::vector<std::uint32_t> query_sources(const WorkloadArtifact& artifact) {
  std::vector<std::uint32_t> result;
  for (const auto& op : artifact.operations)
    if (op.kind == TraceOpKind::Query) result.push_back(op.a);
  return result;
}

static void check_magnitude(const WorkloadArtifact& artifact,
                            Weight min_delta,
                            Weight max_delta) {
  for (const auto& op : updates(artifact)) {
    const auto delta = op.new_weight > op.old_weight
                           ? op.new_weight - op.old_weight
                           : op.old_weight - op.new_weight;
    require(delta >= min_delta && delta <= max_delta,
            "update perturbation escaped configured magnitude band");
  }
}

int main() {
  try {
    require(sha256_bytes("abc") ==
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "generic SHA-256 test vector failed");
    const std::string hash_path = "/tmp/ades-workload-v2-hash-test.bin";
    {
      std::ofstream out(hash_path, std::ios::binary | std::ios::trunc);
      out << "abc";
    }
    require(file_sha256(hash_path) == sha256_bytes("abc"),
            "file SHA-256 differs from byte SHA-256");
    std::remove(hash_path.c_str());

    auto graph = make_graph();
    WorkloadConfig cfg;
    cfg.query_count = 40;
    cfg.update_interval = 0;
    cfg.hot_sources = 4;
    cfg.epoch_queries = 5;
    cfg.locality_percent = 100;

    const SourceFamily families[] = {
        SourceFamily::Uniform, SourceFamily::SingleHot, SourceFamily::HotPool,
        SourceFamily::Zipf, SourceFamily::RotatingHot, SourceFamily::Churn};
    for (auto family : families) {
      cfg.source_family = family;
      const auto a = generate_workload_v2(graph, 77, cfg);
      const auto b = generate_workload_v2(graph, 77, cfg);
      require(canonical_trace_bytes(a.operations) == canonical_trace_bytes(b.operations),
              "same seed+graph+config was not byte deterministic");
      require(a.trace_sha256 == b.trace_sha256,
              "same deterministic trace produced different SHA-256");
      require(a.config_sha256 == b.config_sha256,
              "same config produced different config SHA-256");
      require(a.counts.query_count == cfg.query_count && a.counts.update_count == 0,
              "static workload operation counts are wrong");

      const auto sources = query_sources(a);
      std::set<std::uint32_t> distinct(sources.begin(), sources.end());
      if (family == SourceFamily::SingleHot)
        require(distinct.size() == 1, "single-hot locality=100 used multiple sources");
      if (family == SourceFamily::HotPool || family == SourceFamily::Zipf ||
          family == SourceFamily::RotatingHot || family == SourceFamily::Churn)
        require(distinct.size() <= cfg.hot_sources,
                "localized family escaped its hot source set");
      if (family == SourceFamily::Churn) {
        std::set<std::uint32_t> first_cycle(sources.begin(),
                                            sources.begin() + cfg.hot_sources);
        require(first_cycle.size() == cfg.hot_sources,
                "churn did not rotate through distinct hot sources");
      }
    }

    cfg.source_family = SourceFamily::HotPool;
    const auto seed_a = generate_workload_v2(graph, 77, cfg);
    const auto seed_b = generate_workload_v2(graph, 78, cfg);
    require(canonical_trace_bytes(seed_a.operations) != canonical_trace_bytes(seed_b.operations),
            "different seeds unexpectedly produced identical traces");

    cfg.query_count = 30;
    cfg.update_interval = 2;
    cfg.locality_percent = 90;
    cfg.magnitude = PerturbationMagnitude::Small;
    cfg.burst_length = 3;
    const auto expected_updates = (cfg.query_count - 1) / cfg.update_interval;

    const UpdateMode modes[] = {
        UpdateMode::IncreaseOnly, UpdateMode::DecreaseOnly,
        UpdateMode::BalancedRandom, UpdateMode::StrictAlternating,
        UpdateMode::Bursty, UpdateMode::RepeatedEdge};
    for (auto mode : modes) {
      cfg.update_mode = mode;
      const auto artifact = generate_workload_v2(graph, 91, cfg);
      require(artifact.counts.query_count == cfg.query_count,
              "dynamic workload query count mismatch");
      require(artifact.counts.update_count == expected_updates,
              "dynamic workload update count mismatch");
      check_magnitude(artifact, 1, 3);
      if (mode == UpdateMode::IncreaseOnly) {
        require(artifact.counts.increase_count == expected_updates &&
                    artifact.counts.decrease_count == 0,
                "increase-only direction invariant failed");
      }
      if (mode == UpdateMode::DecreaseOnly) {
        require(artifact.counts.decrease_count == expected_updates &&
                    artifact.counts.increase_count == 0,
                "decrease-only direction invariant failed");
      }
      if (mode == UpdateMode::StrictAlternating || mode == UpdateMode::Bursty ||
          mode == UpdateMode::RepeatedEdge) {
        require(artifact.counts.increase_count + artifact.counts.decrease_count ==
                    expected_updates,
                "alternating mode emitted unchanged update");
        const auto diff = artifact.counts.increase_count > artifact.counts.decrease_count
                              ? artifact.counts.increase_count - artifact.counts.decrease_count
                              : artifact.counts.decrease_count - artifact.counts.increase_count;
        require(diff <= 1, "alternating update directions are imbalanced");
      }
      if (mode == UpdateMode::Bursty) {
        bool adjacent_updates = false;
        for (std::size_t i = 1; i < artifact.operations.size(); ++i)
          adjacent_updates = adjacent_updates ||
                             (artifact.operations[i - 1].kind == TraceOpKind::Update &&
                              artifact.operations[i].kind == TraceOpKind::Update);
        require(adjacent_updates, "bursty mode did not cluster updates");
      }
      if (mode == UpdateMode::RepeatedEdge) {
        const auto us = updates(artifact);
        require(!us.empty(), "repeated-edge produced no updates");
        for (const auto& op : us)
          require(op.a == us.front().a, "repeated-edge changed edge id");
        for (std::size_t i = 0; i < us.size(); ++i) {
          if ((i % 2) == 0) require(us[i].new_weight > us[i].old_weight,
                                   "repeated-edge even update was not increase");
          else require(us[i].new_weight < us[i].old_weight,
                       "repeated-edge odd update was not decrease");
        }
      }
    }

    cfg.update_mode = UpdateMode::StrictAlternating;
    cfg.magnitude = PerturbationMagnitude::Medium;
    check_magnitude(generate_workload_v2(graph, 101, cfg), 4, 31);
    cfg.magnitude = PerturbationMagnitude::Large;
    check_magnitude(generate_workload_v2(graph, 101, cfg), 32, 255);

    WorkloadConfig invalid = cfg;
    invalid.update_interval = 3;
    require_throws([&] { (void)generate_workload_v2(graph, 1, invalid); },
                   "unsupported update interval was accepted");
    invalid = cfg;
    invalid.locality_percent = 101;
    require_throws([&] { (void)generate_workload_v2(graph, 1, invalid); },
                   "invalid locality was accepted");
    invalid = cfg;
    invalid.hot_sources = graph.vertex_count() + 1;
    require_throws([&] { (void)generate_workload_v2(graph, 1, invalid); },
                   "oversized hot pool was accepted");
    invalid = cfg;
    invalid.burst_length = 0;
    require_throws([&] { (void)generate_workload_v2(graph, 1, invalid); },
                   "zero burst length was accepted");

    const auto canonical = canonical_workload_config(cfg);
    require(canonical.find("workload_generator_version=2\n") == 0,
            "canonical config lacks generator version");
    require(canonical.back() == '\n', "canonical config must end with newline");

    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "workload v2 test failure: %s\n", error.what());
    return 1;
  }
}
