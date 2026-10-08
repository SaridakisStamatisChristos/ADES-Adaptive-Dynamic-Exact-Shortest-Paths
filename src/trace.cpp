#include "ades/trace.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace ades {
namespace {

constexpr std::array<std::uint32_t, 64> kSha256Round = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4bU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

constexpr std::uint32_t rotr(std::uint32_t v, unsigned n) {
  return (v >> n) | (v << (32U - n));
}

class Sha256 {
 public:
  Sha256()
      : state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
               0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U} {}

  void update(std::string_view data) {
    for (unsigned char byte : data) {
      block_[block_size_++] = byte;
      if (block_size_ == block_.size()) {
        transform(block_.data());
        processed_bits_ += 512;
        block_size_ = 0;
      }
    }
  }

  std::array<std::uint8_t, 32> finish() {
    const std::uint64_t total_bits =
        processed_bits_ + static_cast<std::uint64_t>(block_size_) * 8U;

    block_[block_size_++] = 0x80U;
    if (block_size_ > 56) {
      while (block_size_ < block_.size()) block_[block_size_++] = 0;
      transform(block_.data());
      block_size_ = 0;
    }
    while (block_size_ < 56) block_[block_size_++] = 0;
    for (int i = 7; i >= 0; --i) {
      block_[block_size_++] =
          static_cast<std::uint8_t>((total_bits >> (i * 8)) & 0xffU);
    }
    transform(block_.data());

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < state_.size(); ++i) {
      digest[4 * i] = static_cast<std::uint8_t>(state_[i] >> 24U);
      digest[4 * i + 1] = static_cast<std::uint8_t>(state_[i] >> 16U);
      digest[4 * i + 2] = static_cast<std::uint8_t>(state_[i] >> 8U);
      digest[4 * i + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    return digest;
  }

 private:
  void transform(const std::uint8_t* data) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(data[4 * i]) << 24U) |
             (static_cast<std::uint32_t>(data[4 * i + 1]) << 16U) |
             (static_cast<std::uint32_t>(data[4 * i + 2]) << 8U) |
             static_cast<std::uint32_t>(data[4 * i + 3]);
    }
    for (std::size_t i = 16; i < w.size(); ++i) {
      const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^
                      (w[i - 15] >> 3U);
      const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^
                      (w[i - 2] >> 10U);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];

    for (std::size_t i = 0; i < 64; ++i) {
      const auto s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const auto ch = (e & f) ^ ((~e) & g);
      const auto t1 = h + s1 + ch + kSha256Round[i] + w[i];
      const auto s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const auto maj = (a & b) ^ (a & c) ^ (b & c);
      const auto t2 = s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::array<std::uint32_t, 8> state_;
  std::array<std::uint8_t, 64> block_{};
  std::size_t block_size_{0};
  std::uint64_t processed_bits_{0};
};

template <typename UInt>
void append_uint(std::string& out, UInt value) {
  std::array<char, 32> buf{};
  const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
  if (ec != std::errc{}) throw std::runtime_error("integer serialization failed");
  out.append(buf.data(), end);
}

std::string digest_hex(const std::array<std::uint8_t, 32>& digest) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto byte : digest) out << std::setw(2) << static_cast<unsigned>(byte);
  return out.str();
}

std::uint32_t checked_u32(std::uint64_t value, const char* field) {
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error(std::string(field) + " exceeds uint32 range");
  }
  return static_cast<std::uint32_t>(value);
}

}  // namespace

std::string sha256_bytes(std::string_view bytes) {
  Sha256 sha;
  sha.update(bytes);
  return digest_hex(sha.finish());
}

std::string file_sha256(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot hash file: " + path);
  Sha256 sha;
  std::array<char, 64 * 1024> buffer{};
  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = in.gcount();
    if (count > 0) sha.update(std::string_view(buffer.data(), static_cast<std::size_t>(count)));
  }
  if (!in.eof()) throw std::runtime_error("failed while hashing file: " + path);
  return digest_hex(sha.finish());
}

std::string canonical_trace_bytes(const std::vector<TraceOp>& ops) {
  std::string out;
  out.reserve(ops.size() * 32);
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Query) {
      out += "QUERY ";
      append_uint(out, op.a);
      out.push_back(' ');
      append_uint(out, op.b);
      out.push_back('\n');
    } else if (op.kind == TraceOpKind::Update) {
      out += "UPDATE ";
      append_uint(out, op.a);
      out.push_back(' ');
      append_uint(out, op.old_weight);
      out.push_back(' ');
      append_uint(out, op.new_weight);
      out.push_back('\n');
    } else {
      throw std::runtime_error("unknown trace operation kind");
    }
  }
  return out;
}

std::string trace_sha256(const std::vector<TraceOp>& ops) {
  return sha256_bytes(canonical_trace_bytes(ops));
}

TraceCounts trace_counts(const std::vector<TraceOp>& ops) {
  TraceCounts counts;
  for (const auto& op : ops) {
    if (op.kind == TraceOpKind::Query) {
      ++counts.query_count;
    } else if (op.kind == TraceOpKind::Update) {
      ++counts.update_count;
      if (op.new_weight > op.old_weight) ++counts.increase_count;
      if (op.new_weight < op.old_weight) ++counts.decrease_count;
    } else {
      throw std::runtime_error("unknown trace operation kind");
    }
  }
  return counts;
}

void write_trace(const std::string& path, const std::vector<TraceOp>& ops) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write trace: " + path);
  const auto bytes = canonical_trace_bytes(ops);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!out) throw std::runtime_error("failed while writing trace: " + path);
}

std::vector<TraceOp> read_trace(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read trace: " + path);

  std::vector<TraceOp> ops;
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) {
      throw std::runtime_error("empty trace line at " + std::to_string(line_no));
    }

    std::istringstream row(line);
    std::string kind;
    row >> kind;
    if (kind == "QUERY") {
      std::uint64_t source = 0, target = 0;
      std::string extra;
      if (!(row >> source >> target) || (row >> extra)) {
        throw std::runtime_error("bad QUERY at trace line " +
                                 std::to_string(line_no));
      }
      ops.push_back(TraceOp::query(checked_u32(source, "source"),
                                   checked_u32(target, "target")));
    } else if (kind == "UPDATE") {
      std::uint64_t edge = 0;
      Weight old_weight = 0, new_weight = 0;
      std::string extra;
      if (!(row >> edge >> old_weight >> new_weight) || (row >> extra)) {
        throw std::runtime_error("bad UPDATE at trace line " +
                                 std::to_string(line_no));
      }
      ops.push_back(TraceOp::update(checked_u32(edge, "edge_id"), old_weight,
                                    new_weight));
    } else {
      throw std::runtime_error("unknown trace operation at line " +
                               std::to_string(line_no));
    }
  }
  if (!in.eof()) throw std::runtime_error("failed while reading trace: " + path);
  return ops;
}

void validate_trace_against_graph(const Graph& initial,
                                  const std::vector<TraceOp>& ops) {
  Graph graph = initial;
  for (std::size_t i = 0; i < ops.size(); ++i) {
    const auto& op = ops[i];
    if (op.kind == TraceOpKind::Query) {
      if (op.a >= graph.vertex_count() || op.b >= graph.vertex_count()) {
        throw std::runtime_error("trace query vertex out of range at operation " +
                                 std::to_string(i));
      }
      continue;
    }
    if (op.kind != TraceOpKind::Update) {
      throw std::runtime_error("unknown trace operation kind at operation " +
                               std::to_string(i));
    }
    if (op.a >= graph.edge_count()) {
      throw std::runtime_error("trace edge id out of range at operation " +
                               std::to_string(i));
    }
    const auto actual = graph.edge(op.a).weight;
    if (actual != op.old_weight) {
      throw std::runtime_error("trace old_weight mismatch at operation " +
                               std::to_string(i) + ": expected " +
                               std::to_string(actual) + ", recorded " +
                               std::to_string(op.old_weight));
    }
    graph.update_weight(op.a, op.new_weight);
  }
}

}  // namespace ades
