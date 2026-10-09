#include "ades/progressive_frontier.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace ades {
namespace {

constexpr std::int64_t kUnseen = -1;
constexpr std::int64_t kSettled = -2;

bool heap_less(const ProgressiveFrontier& f, std::uint32_t a,
               std::uint32_t b) {
  if (f.dist[a] != f.dist[b]) return f.dist[a] < f.dist[b];
  return a < b;
}

void heap_swap(ProgressiveFrontier& f, std::size_t a, std::size_t b) {
  std::swap(f.heap_vertices[a], f.heap_vertices[b]);
  f.heap_pos[f.heap_vertices[a]] = static_cast<std::int64_t>(a);
  f.heap_pos[f.heap_vertices[b]] = static_cast<std::int64_t>(b);
}

void sift_up(ProgressiveFrontier& f, std::size_t index) {
  while (index) {
    const auto parent = (index - 1) / 2;
    if (!heap_less(f, f.heap_vertices[index], f.heap_vertices[parent])) break;
    heap_swap(f, index, parent);
    index = parent;
  }
}

void sift_down(ProgressiveFrontier& f, std::size_t index) {
  for (;;) {
    const auto left = index * 2 + 1;
    const auto right = left + 1;
    auto best = index;
    if (left < f.heap_vertices.size() &&
        heap_less(f, f.heap_vertices[left], f.heap_vertices[best]))
      best = left;
    if (right < f.heap_vertices.size() &&
        heap_less(f, f.heap_vertices[right], f.heap_vertices[best]))
      best = right;
    if (best == index) break;
    heap_swap(f, index, best);
    index = best;
  }
}

void push_or_decrease(ProgressiveFrontier& f, std::uint32_t vertex,
                      Distance distance, std::int64_t parent_edge) {
  auto& pos = f.heap_pos[vertex];
  if (pos == kSettled) {
    if (distance < f.dist[vertex])
      throw std::logic_error("progressive Dijkstra attempted to improve settled vertex");
    return;
  }
  if (distance >= f.dist[vertex]) return;
  f.dist[vertex] = distance;
  f.parent_edge[vertex] = parent_edge;
  if (pos == kUnseen) {
    f.heap_vertices.push_back(vertex);
    pos = static_cast<std::int64_t>(f.heap_vertices.size() - 1);
  }
  sift_up(f, static_cast<std::size_t>(pos));
}

std::uint32_t pop_min(ProgressiveFrontier& f) {
  if (f.heap_vertices.empty())
    throw std::logic_error("pop from empty progressive heap");
  const auto root = f.heap_vertices.front();
  const auto last = f.heap_vertices.back();
  f.heap_vertices.pop_back();
  f.heap_pos[root] = kSettled;
  if (!f.heap_vertices.empty()) {
    f.heap_vertices.front() = last;
    f.heap_pos[last] = 0;
    sift_down(f, 0);
  }
  ++f.settled_vertices;
  return root;
}

void expand_forward(const Graph& graph, ProgressiveFrontier& f,
                    std::uint32_t vertex) {
  const auto du = f.dist[vertex];
  for (const auto arc : graph.out(vertex)) {
    ++f.forward_edge_scans;
    const auto nd = sat_add(du, graph.edge(arc.edge_id).weight);
    push_or_decrease(f, arc.to, nd,
                     static_cast<std::int64_t>(arc.edge_id));
  }
}

}  // namespace

ProgressiveFrontier::ProgressiveFrontier(std::size_t vertex_count,
                                         std::uint32_t source_vertex)
    : source(source_vertex),
      dist(vertex_count, INF),
      parent_edge(vertex_count, -1),
      heap_pos(vertex_count, kUnseen) {
  if (source_vertex >= vertex_count)
    throw std::out_of_range("progressive source vertex");
  heap_vertices.reserve(vertex_count);
  dist[source_vertex] = 0;
  heap_vertices.push_back(source_vertex);
  heap_pos[source_vertex] = 0;
}

ProgressiveQueryResult progressive_bidirectional_query(
    const Graph& graph, ProgressiveFrontier& f, std::uint32_t target) {
  if (target >= graph.vertex_count())
    throw std::out_of_range("progressive target vertex");
  if (f.dist.size() != graph.vertex_count() ||
      f.parent_edge.size() != graph.vertex_count() ||
      f.heap_pos.size() != graph.vertex_count())
    throw std::invalid_argument("progressive frontier graph shape mismatch");

  ProgressiveQueryResult result;
  if (target == f.source) {
    result.distance = 0;
    return result;
  }
  if (f.heap_pos[target] == kSettled || f.complete) {
    result.distance = f.dist[target];
    return result;
  }

  using Pair = std::pair<Distance, std::uint32_t>;
  std::vector<Distance> backward(graph.vertex_count(), INF);
  std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> qb;
  backward[target] = 0;
  qb.push({0, target});

  Distance best = f.dist[target];
  const bool was_complete = f.complete;

  while (!qb.empty()) {
    if (f.heap_vertices.empty()) {
      f.complete = true;
      result.distance = f.dist[target];
      result.became_complete = !was_complete;
      return result;
    }

    while (!qb.empty() && qb.top().first != backward[qb.top().second]) qb.pop();
    if (qb.empty()) break;

    const auto forward_min = f.dist[f.heap_vertices.front()];
    const auto backward_min = qb.top().first;
    if (best < INF && sat_add(forward_min, backward_min) >= best) {
      result.distance = best;
      return result;
    }

    if (forward_min <= backward_min) {
      const auto u = pop_min(f);
      ++result.forward_settled;
      if (u == target) {
        result.distance = f.dist[u];
        return result;
      }
      if (backward[u] < INF)
        best = std::min(best, sat_add(f.dist[u], backward[u]));

      const auto du = f.dist[u];
      for (const auto arc : graph.out(u)) {
        ++result.forward_edge_scans;
        ++f.forward_edge_scans;
        const auto nd = sat_add(du, graph.edge(arc.edge_id).weight);
        push_or_decrease(f, arc.to, nd,
                         static_cast<std::int64_t>(arc.edge_id));
        if (backward[arc.to] < INF && f.dist[arc.to] < INF)
          best = std::min(best, sat_add(f.dist[arc.to], backward[arc.to]));
      }
    } else {
      const auto [du, u] = qb.top();
      qb.pop();
      if (du != backward[u]) continue;
      if (f.dist[u] < INF) best = std::min(best, sat_add(f.dist[u], du));
      for (const auto arc : graph.in(u)) {
        ++result.backward_edge_scans;
        const auto nd = sat_add(du, graph.edge(arc.edge_id).weight);
        if (nd < backward[arc.to]) {
          backward[arc.to] = nd;
          qb.push({nd, arc.to});
        }
        if (f.dist[arc.to] < INF)
          best = std::min(best, sat_add(f.dist[arc.to], nd));
      }
    }
  }

  result.distance = best;
  return result;
}

SSSPState complete_progressive_sssp(const Graph& graph,
                                    ProgressiveFrontier& f) {
  if (f.dist.size() != graph.vertex_count())
    throw std::invalid_argument("progressive frontier graph shape mismatch");

  while (!f.heap_vertices.empty()) {
    const auto u = pop_min(f);
    expand_forward(graph, f, u);
  }
  f.complete = true;

  const auto n = graph.vertex_count();
  SSSPState state;
  state.source = f.source;
  state.dist = std::move(f.dist);
  state.parent_edge = std::move(f.parent_edge);
  state.first_child.assign(n, -1);
  state.next_sibling.assign(n, -1);
  state.prev_sibling.assign(n, -1);
  state.repair_mark.assign(n, 0);
  state.repair_epoch = 0;

  for (std::uint32_t v = 0; v < n; ++v) {
    const auto parent = state.parent_edge[v];
    if (parent < 0) continue;
    const auto p = graph.edge(static_cast<std::uint32_t>(parent)).from;
    const auto first = state.first_child[p];
    state.next_sibling[v] = first;
    if (first >= 0)
      state.prev_sibling[static_cast<std::uint32_t>(first)] = v;
    state.first_child[p] = v;
  }
  f.heap_pos.clear();
  f.heap_vertices.clear();
  return state;
}

}  // namespace ades
