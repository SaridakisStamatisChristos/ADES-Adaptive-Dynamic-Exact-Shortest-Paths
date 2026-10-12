#include "ades/radix_queue.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ades;

static void basic_order_and_equal_keys() {
  RadixQueue q;
  q.push(10, 1); q.push(3, 2); q.push(3, 3); q.push(100, 4); q.push(0, 5);
  Distance previous = 0;
  std::vector<Distance> keys;
  while (!q.empty()) {
    const auto [key, value] = q.pop();
    (void)value;
    assert(key >= previous);
    previous = key;
    keys.push_back(key);
  }
  assert((keys == std::vector<Distance>{0,3,3,10,100}));
}

static void monotone_guard() {
  RadixQueue q;
  q.push(5, 1); q.push(9, 2);
  assert(q.pop().first == 5);
  bool rejected = false;
  try { q.push(4, 3); } catch (const std::logic_error&) { rejected = true; }
  assert(rejected);
  q.push(5, 4);
  assert(q.pop().first == 5);
  assert(q.pop().first == 9);
}

static void clear_resets_epoch_key() {
  RadixQueue q;
  q.push(1000, 1); assert(q.pop().first == 1000);
  q.clear();
  q.push(1, 2); assert(q.min_key() == 1); assert(q.pop().second == 2);
}

static void randomized_against_binary_heap() {
  using Item = std::pair<Distance,std::uint32_t>;
  std::mt19937_64 rng(0xA55A1234ULL);
  for (int round = 0; round < 100; ++round) {
    RadixQueue radix;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> binary;
    for (std::uint32_t i = 0; i < 500; ++i) {
      const Distance key = static_cast<Distance>(rng() % 1000000ULL);
      radix.push(key, i); binary.push({key, i});
    }
    Distance last = 0;
    std::uint32_t next_value = 500;
    while (!binary.empty()) {
      const auto expected = binary.top(); binary.pop();
      const auto got = radix.pop();
      assert(got.first == expected.first);
      assert(got.first >= last);
      last = got.first;
      if ((rng() & 7ULL) == 0 && next_value < 900) {
        const Distance key = last + static_cast<Distance>(rng() % 10000ULL);
        radix.push(key, next_value); binary.push({key, next_value}); ++next_value;
      }
    }
    assert(radix.empty());
  }
}

int main() {
  basic_order_and_equal_keys();
  monotone_guard();
  clear_resets_epoch_key();
  randomized_against_binary_heap();
}
