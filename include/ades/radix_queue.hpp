#pragma once

#include "ades/graph.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ades {

// Monotone radix queue for nonnegative 64-bit Dijkstra keys.
// Duplicate entries are permitted; callers keep the ordinary stale-label check.
class RadixQueue {
 public:
  using Item = std::pair<Distance, std::uint32_t>;

 private:
  std::array<std::vector<Item>, 65> buckets_{};
  std::vector<Item> scratch_{};
  std::size_t size_ = 0;
  Distance last_ = 0;

  static std::size_t bucket_index(Distance key, Distance last) {
    const auto diff = key ^ last;
    if (!diff) return 0;
    return static_cast<std::size_t>(64 - std::countl_zero(diff));
  }

  void pull_min_bucket() {
    if (!buckets_[0].empty() || size_ == 0) return;
    std::size_t index = 1;
    while (index < buckets_.size() && buckets_[index].empty()) ++index;
    if (index == buckets_.size()) throw std::logic_error("radix queue size invariant violated");

    Distance next = buckets_[index].front().first;
    for (const auto& item : buckets_[index]) next = std::min(next, item.first);
    last_ = next;

    scratch_.clear();
    scratch_.swap(buckets_[index]);
    for (const auto& item : scratch_)
      buckets_[bucket_index(item.first, last_)].push_back(item);
    scratch_.clear();
  }

 public:
  bool empty() const noexcept { return size_ == 0; }
  std::size_t size() const noexcept { return size_; }
  Distance last_key() const noexcept { return last_; }

  void clear() {
    for (auto& bucket : buckets_) bucket.clear();
    scratch_.clear();
    size_ = 0;
    last_ = 0;
  }

  void push(Distance key, std::uint32_t value) {
    if (key < last_) throw std::logic_error("radix queue received non-monotone key");
    buckets_[bucket_index(key, last_)].push_back({key, value});
    ++size_;
  }

  Distance min_key() {
    if (!size_) throw std::out_of_range("min_key on empty radix queue");
    pull_min_bucket();
    return buckets_[0].back().first;
  }

  Item pop() {
    if (!size_) throw std::out_of_range("pop on empty radix queue");
    pull_min_bucket();
    const auto item = buckets_[0].back();
    buckets_[0].pop_back();
    --size_;
    return item;
  }
};

} // namespace ades
