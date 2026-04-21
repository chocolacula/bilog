#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "buffer.hpp"

namespace bilog {

/// @brief Sink that writes log records to a file via `std::ofstream`.
///
/// Writes stage into the Logger's `bilog::Buffer` and transfer to the
/// underlying `std::ofstream` when the staging buffer would overflow.
///
/// Multiple threads can share one FileSink safely, file I/O is thread safe.
///
/// @note A default-constructed FileSink has no file open, all writes are discarded.
class FileSink {
  std::ofstream file_;
  std::mutex file_mutex_;

 public:
  static constexpr std::size_t kBuffCap = 8U * 1024L;

  FileSink() = default;
  ~FileSink() = default;

  explicit FileSink(const std::filesystem::path& path)
      : file_(path, std::ios::binary | std::ios::trunc) {}
  explicit FileSink(std::ofstream&& stream) : file_(std::move(stream)) {}

  FileSink(FileSink&& other) noexcept = delete;
  FileSink& operator=(FileSink&& other) noexcept {
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

  void commit(Buffer<FileSink>* /*lb*/) {}

 private:
  void write_file(Buffer<FileSink>* lb, const std::byte* data, std::size_t size);
};
}  // namespace bilog
