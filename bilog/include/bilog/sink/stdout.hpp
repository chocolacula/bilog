#pragma once

#include <cstddef>
#include <cstdio>
#include <string_view>

#include "buffer.hpp"

namespace bilog {

/// @brief Sink that writes to stdout with ANSI color support.
///
/// Uses the Logger's thread-local Buffer for line staging.
/// On '\n', scans for a level tag at the start, wraps it in ANSI
/// color codes, and writes to stdout. On non-TTY, colors are suppressed.
class StdoutSink {
  bool color_ = false;

  static constexpr std::string_view kColors[] = {
      "\033[37m",    // TRACE - white
      "\033[36m",    // DEBUG - cyan
      "\033[32m",    // INFO  - green
      "\033[33m",    // WARN  - yellow
      "\033[31m",    // ERROR - red
      "\033[1;31m",  // FATAL - bold red
  };
  static constexpr std::string_view kReset = "\033[0m";
  static constexpr std::string_view kTags[] = {
      "[TRACE]",
      "[DEBUG]",
      "[INFO]",
      "[WARN]",
      "[ERROR]",
      "[FATAL]",
  };

 public:
  static constexpr std::size_t kBuffCap = 512;

  StdoutSink();
  ~StdoutSink() = default;

  StdoutSink(StdoutSink&&) noexcept = default;
  StdoutSink& operator=(StdoutSink&&) noexcept = default;
  StdoutSink(const StdoutSink&) = delete;
  StdoutSink& operator=(const StdoutSink&) = delete;

  void write(Buffer<StdoutSink>* lb, const std::byte* data, std::size_t size) {
    lb->append(data, size);
  }

  void write_byte(Buffer<StdoutSink>* lb, std::byte b) {
    lb->append(b);
  }

  void commit(Buffer<StdoutSink>* lb) {
    flush(lb);
  }

  void flush(Buffer<StdoutSink>* lb);
};

}  // namespace bilog
