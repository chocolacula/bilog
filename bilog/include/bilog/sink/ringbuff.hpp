#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

#include "buffer.hpp"

namespace bilog {

/// @brief Lock-free multi-producer ring buffer sink.
///
/// Uses the Logger's thread-local Buffer for line staging.
/// On flush(), the entire line is committed atomically to the shared
/// ring buffer via a single fetch_add. This prevents interleaving
/// of bytes from different threads.
class RingBuffSink {
  static constexpr std::size_t kDefaultCap = 64UL * 1024UL;

  std::vector<std::byte> ring_;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};

 public:
  static constexpr std::size_t kBuffCap = 512;

  RingBuffSink() : ring_(kDefaultCap) {}
  explicit RingBuffSink(std::size_t size) : ring_(size) {}

  RingBuffSink(RingBuffSink&& other) noexcept = delete;
  RingBuffSink& operator=(RingBuffSink&& other) noexcept {
    if (this != &other) {
      ring_ = std::move(other.ring_);
      head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      tail_.store(other.tail_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      other.head_.store(0, std::memory_order_relaxed);
      other.tail_.store(0, std::memory_order_relaxed);
    }
    return *this;
  }

  RingBuffSink(const RingBuffSink&) = delete;
  RingBuffSink& operator=(const RingBuffSink&) = delete;

  ~RingBuffSink() = default;

  void write(Buffer<RingBuffSink>* lb, const std::byte* data, std::size_t size) {
    lb->append(data, size);
    if (size > 0 && data[size - 1] == static_cast<std::byte>('\n')) {
      flush(lb);
    }
  }

  void write_byte(Buffer<RingBuffSink>* lb, std::byte b) {
    lb->append(b);
    if (b == static_cast<std::byte>('\n')) {
      flush(lb);
    }
  }

  void flush(Buffer<RingBuffSink>* lb) {
    if (lb->size() == 0 || ring_.empty()) {
      return;
    }

    auto len = lb->size();
    auto start = head_.fetch_add(len, std::memory_order_relaxed);

    auto offset = start % ring_.size();
    auto first = std::min(len, ring_.size() - offset);
    std::memcpy(&ring_[offset], lb->data(), first);
    if (first < len) {
      std::memcpy(ring_.data(), lb->data() + first, len - first);
    }

    auto tail = tail_.load(std::memory_order_relaxed);
    if (start + len - tail > ring_.size()) {
      tail_.store(start + len - ring_.size(), std::memory_order_release);
    }
    lb->clear();
  }

  [[nodiscard]] std::size_t available() const {
    auto h = head_.load(std::memory_order_acquire);
    auto t = tail_.load(std::memory_order_acquire);
    return h - t;
  }

  std::size_t read(std::byte* out, std::size_t size) {
    auto h = head_.load(std::memory_order_acquire);
    auto t = tail_.load(std::memory_order_relaxed);
    auto avail = h - t;
    auto to_read = avail < size ? avail : size;

    for (std::size_t i = 0; i < to_read; ++i) {
      out[i] = ring_[(t + i) % ring_.size()];
    }
    tail_.store(t + to_read, std::memory_order_release);
    return to_read;
  }

  void clear() {
    tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
  }

  static constexpr std::size_t capacity() { return kDefaultCap; }
};

}  // namespace bilog
