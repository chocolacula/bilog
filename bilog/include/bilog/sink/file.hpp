#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "buffer.hpp"

namespace bilog {

class FileSink {
  std::ofstream file_;
  std::mutex file_mutex_;

 public:
  static constexpr std::size_t kBuffCap = 8U * 1024L;

  /// Default constructed FileSink discard all writes
  FileSink() = default;
  ~FileSink() = default;

  explicit FileSink(const std::filesystem::path& path)
      : file_(path, std::ios::binary | std::ios::trunc) {}
  explicit FileSink(std::ofstream&& stream) : file_(std::move(stream)) {}

  FileSink(FileSink&& other) noexcept = delete;
  FileSink& operator=(FileSink&& other) noexcept {  // does not flush
    if (this != &other) {
      file_ = std::move(other.file_);
    }
    return *this;
  }

  FileSink(const FileSink&) = delete;
  FileSink& operator=(const FileSink&) = delete;

  void write(Buffer<FileSink>* lb, const std::byte* data, std::size_t size) {
    if (lb->size() + size <= kBuffCap) [[likely]] {
      lb->append_unsafe(data, size);
      return;
    }
    write_file(lb, data, size);
  }

  void write_byte(Buffer<FileSink>* lb, std::byte b) {
    if (lb->size() < kBuffCap) [[likely]] {
      lb->append_unsafe(b);
      return;
    }
    write_file(lb, &b, 1);
  }

  void flush(Buffer<FileSink>* lb);

  /// End-of-record commit: file sink batches across records, so this is a
  /// no-op. The staging buffer flushes when it fills up or on destruction.
  void commit(Buffer<FileSink>* /*lb*/) {}

 private:
  void write_file(Buffer<FileSink>* lb, const std::byte* data, std::size_t size);
};
}  // namespace bilog
