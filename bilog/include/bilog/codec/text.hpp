#pragma once

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "bilog/level.hpp"
#include "bilog/sink/buffer.hpp"
#include "bilog/tag.hpp"

namespace bilog {

/// @brief Text encoder — writes human-readable log lines directly to a sink.
///
/// Same encode_pair interface as BinaryEncoder, but outputs text instead of
/// binary. No schema or postprocessor needed — the output is the final log.
///
/// Output per record: [LEVEL] message field: value field: value\n
class TextEncoder {
  std::size_t pair_index_ = 0;

 public:
  TextEncoder() = default;

  /// Pair 0: [event_id, level] — write "[LEVEL] "
  template <typename SinkT, std::integral A>
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& /*event_id*/, std::byte level) {
    write_level_header(lb, sink, level);
    ++pair_index_;
  }

  /// All other fixed-size pairs (Tag+int, Tag+float, Tag+Tag, Tag+byte).
  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> &&
             !std::convertible_to<B, std::string_view> && !std::integral<std::remove_cvref_t<A>>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    if (pair_index_ == 1) {
      write_tag_str(lb, sink, a);
    } else {
      write_byte(lb, sink, ' ');
      write_tag_str(lb, sink, a);
      write_byte(lb, sink, ' ');
      write_value(lb, sink, b);
    }
    ++pair_index_;
  }

  /// Pair where the second value is a string.
  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> && std::convertible_to<B, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    write_byte(lb, sink, ' ');
    write_tag_str(lb, sink, a);
    write_byte(lb, sink, ' ');
    write_str(lb, sink, std::string_view(b));
    ++pair_index_;
  }

  template <typename SinkT>
  void finish(Buffer<SinkT>* lb, SinkT* sink) {
    sink->write_byte(lb, static_cast<std::byte>('\n'));
    sink->commit(lb);
    pair_index_ = 0;
  }

 private:
  template <typename SinkT>
  void write_level_header(Buffer<SinkT>* lb, SinkT* sink, std::byte level_byte) {
    auto lvl = Level::from_byte(level_byte);
    write_byte(lb, sink, '[');
    if (lvl) {
      auto name = lvl->to_str();
      if (name) {
        write_str(lb, sink, *name);
      }
    }
    write_byte(lb, sink, ']');
    write_byte(lb, sink, ' ');
  }

  template <typename SinkT>
  static void write_tag_str(Buffer<SinkT>* lb, SinkT* sink, const Tag& tag) {
    write_str(lb, sink, tag.str());
  }

  template <typename SinkT, typename T>
  static void write_tag_str(Buffer<SinkT>* /*lb*/, SinkT* /*sink*/, const T& /*val*/) {}

  template <typename SinkT>
  static void write_value(Buffer<SinkT>* lb, SinkT* sink, bool val) {
    write_str(lb, sink, val ? std::string_view("true") : std::string_view("false"));
  }

  template <typename SinkT, std::integral T>
  static void write_value(Buffer<SinkT>* lb, SinkT* sink, const T& val) {
    char buf[20];
    auto result = std::to_chars(buf, buf + sizeof(buf), val);
    write_str(lb, sink, std::string_view(buf, static_cast<std::size_t>(result.ptr - buf)));
  }

  template <typename SinkT, std::floating_point T>
  static void write_value(Buffer<SinkT>* lb, SinkT* sink, const T& val) {
    char buf[32];
    auto len = std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(val));
    if (len > 0) {
      write_str(lb, sink, std::string_view(buf, static_cast<std::size_t>(len)));
    }
  }

  template <typename SinkT>
  static void write_value(Buffer<SinkT>* lb, SinkT* sink, const Tag& tag) {
    write_str(lb, sink, tag.str());
  }

  template <typename SinkT>
  static void write_value(Buffer<SinkT>* lb, SinkT* sink, std::byte val) {
    write_value(lb, sink, static_cast<std::uint8_t>(val));
  }

  template <typename SinkT>
  static void write_str(Buffer<SinkT>* lb, SinkT* sink, std::string_view s) {
    sink->write(lb, reinterpret_cast<const std::byte*>(s.data()), s.size());
  }

  template <typename SinkT>
  static void write_byte(Buffer<SinkT>* lb, SinkT* sink, char c) {
    sink->write_byte(lb, static_cast<std::byte>(c));
  }
};

}  // namespace bilog
