#pragma once

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "bilog/sink/buffer.hpp"
#include "bilog/sink/ringbuff.hpp"

namespace bilog::test {

/// Build a path in the system temp directory, prefixed to isolate a test suite.
inline std::filesystem::path temp_path(const char* name) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<std::uint32_t> dist;

  auto dir = std::filesystem::temp_directory_path() / "bilog_test";
  std::filesystem::create_directories(dir);

  return dir / std::format("{:08x}_{}", dist(rng), name);
}

inline void write_file(const std::filesystem::path& path, std::string_view content) {
  std::ofstream out(path);
  out << content;
}

/// Flush staging buffer and drain ring buffer into a contiguous byte vector.
inline std::vector<std::byte> drain(Buffer<RingBuffSink>* buf, RingBuffSink* sink) {
  sink->flush(buf);
  std::vector<std::byte> out(sink->available());
  sink->read(out.data(), out.size());
  return out;
}

/// Flush staging buffer and drain ring buffer into a string.
inline std::string drain_str(Buffer<RingBuffSink>* buf, RingBuffSink* sink) {
  sink->flush(buf);
  std::string result(sink->available(), '\0');
  sink->read(reinterpret_cast<std::byte*>(result.data()), result.size());
  return result;
}

}  // namespace bilog::test
