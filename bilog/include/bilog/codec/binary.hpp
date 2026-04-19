#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bilog/codec/typecode.hpp"
#include "bilog/level.hpp"
#include "bilog/sink/buffer.hpp"

namespace bilog {

/// @brief Encodes arguments into paired binary format.
///
/// Wire layout per pair: [header][payload_a][payload_b]
///   header = [type_a:4][type_b:4]
///
/// A dynamic string could be only the last argument in the pair.
class BinaryEncoder {
 public:
  BinaryEncoder() = default;

  /// Encode a pair of fixed-size values (int, float, Tag, byte).
  /// Batches header + both payloads into one sink->write call.
  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> && !std::convertible_to<B, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    std::byte buf[17];
    auto hi = type_code(a);
    auto lo = type_code(b);
    buf[0] = static_cast<std::byte>((hi << 4U) | lo);
    std::size_t pos = 1;
    append_payload(buf, &pos, hi, a);
    append_payload(buf, &pos, lo, b);
    sink->write(lb, buf, pos);
  }

  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> && std::convertible_to<B, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    auto sv = std::string_view(b);
    auto len = std::min(sv.size(), static_cast<std::size_t>(0xFFFF));

    auto hi = type_code(a);
    auto lo = len <= 0xFF ? tc_string8 : tc_string16;

    std::byte buf[12];
    buf[0] = static_cast<std::byte>((hi << 4U) | lo);

    std::size_t pos = 1;
    append_payload(buf, &pos, hi, a);

    if (lo == tc_string8) {
      buf[pos++] = static_cast<std::byte>(len);
    } else {
      auto len16 = static_cast<std::uint16_t>(len);
      std::memcpy(buf + pos, &len16, 2);
      pos += 2;
    }
    sink->write(lb, buf, pos);
    sink->write(lb, reinterpret_cast<const std::byte*>(sv.data()), len);
  }

  template <typename SinkT>
  void finish(Buffer<SinkT>* lb, SinkT* sink) {
    sink->write_byte(lb, static_cast<std::byte>('\n'));
  }

 private:
  static void append_payload(std::byte* buf, std::size_t* pos, uint8_t tc, const Tag& tag) {
    append_payload(buf, pos, tc, tag.id());
  }

  static void append_payload(std::byte* buf, std::size_t* pos, uint8_t tc, std::byte val) {
    append_payload(buf, pos, tc, static_cast<std::uint8_t>(val));
  }

  template <std::integral T>
  static void append_payload(std::byte* buf, std::size_t* pos, uint8_t tc, const T& val) {
    auto n = static_cast<std::size_t>(tc + 1);
    std::memcpy(buf + *pos, &val, n);
    *pos += n;
  }

  template <std::floating_point T>
  static void append_payload(std::byte* buf, std::size_t* pos, uint8_t /*tc*/, const T& val) {
    std::memcpy(buf + *pos, &val, sizeof(T));
    *pos += sizeof(T);
  }
};

/// @brief Reads binary log data from a buffer and writes formatted text
/// to a SinkT.
///
/// Wire layout per record:
///   [event_id, level] [msg_tag, 0] [field_tag, value] ... [\n]
///
/// Output per record (written to sink):
///   [LEVEL] message field_name: value field_name: value\n
///
/// Requires a schema mapping tag IDs to strings and event IDs to
/// tag positions in the decoded stream.
class BinaryFormatter {
  const std::byte* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;

 public:
  BinaryFormatter() = default;
  BinaryFormatter(const std::byte* data, std::size_t size) : data_(data), size_(size) {}

  void reset(const std::byte* data, std::size_t size) {
    data_ = data;
    size_ = size;
    pos_ = 0;
  }

  [[nodiscard]] bool has_data() const {
    return pos_ < size_;
  }

  /// Format one record from binary into text written to sink.
  /// tag_names: tag_id -> string (e.g. 101 -> "startup")
  /// event_positions: event_id -> list of flat positions that are tag IDs
  ///
  /// Returns false if buffer exhausted or record is incomplete.
  template <typename SinkT>
  bool format(Buffer<SinkT>& lb, SinkT& sink,
              const std::unordered_map<std::uint64_t, std::string>& tag_names,
              const std::unordered_map<std::uint64_t, std::vector<std::size_t>>& event_positions) {
    if (pos_ >= size_) {
      return false;
    }

    // Decode all values in this record into a flat list.
    std::vector<FormattedValue> values;
    if (!decode_record(values)) {
      return false;
    }

    if (values.size() < 2) {
      return true;  // malformed but skip
    }

    // pos 0 = event_id, pos 1 = level
    auto event_id = values[0].u64;
    auto level_byte = static_cast<std::byte>(values[1].u64);

    // Look up tag positions for this event
    std::set<std::size_t> tag_pos;
    auto ev_it = event_positions.find(event_id);
    if (ev_it != event_positions.end()) {
      for (auto p : ev_it->second) {
        tag_pos.insert(p);
      }
    }

    // Write: [LEVEL]
    sink.write_byte(&lb, static_cast<std::byte>('['));
    auto lvl = Level::from_byte(level_byte);
    if (lvl) {
      auto name = lvl->to_str();
      if (name) {
        write_str(lb, sink, *name);
      }
    }
    sink.write_byte(&lb, static_cast<std::byte>(']'));
    sink.write_byte(&lb, static_cast<std::byte>(' '));

    // Write values starting from pos 2.
    // Position 3 is always a hardcoded placeholder (the 0 after msg_tag).
    bool first = true;
    for (std::size_t i = 2; i < values.size(); ++i) {
      if (i == 3) {
        continue;
      }

      if (!first) {
        sink.write_byte(&lb, static_cast<std::byte>(' '));
      }
      first = false;

      // If this position is a tag, write the tag name
      if (tag_pos.count(i) > 0 && values[i].tc <= tc_uint64) {
        auto tag_it = tag_names.find(values[i].u64);
        if (tag_it != tag_names.end()) {
          write_str(lb, sink, tag_it->second);
        } else {
          write_uint(lb, sink, values[i].u64);
        }
        continue;
      }

      // Otherwise format the value
      write_value(lb, sink, values[i]);
    }

    sink.write_byte(&lb, static_cast<std::byte>('\n'));
    return true;
  }

 private:
  struct FormattedValue {
    std::uint8_t tc = 0;
    std::uint64_t u64 = 0;
    float f32 = 0;
    double f64 = 0;
    std::string_view str;
  };

  bool decode_record(std::vector<FormattedValue>& values) {
    while (pos_ < size_) {
      if (data_[pos_] == static_cast<std::byte>('\n')) {
        ++pos_;
        return true;
      }

      auto byte = static_cast<std::uint8_t>(data_[pos_++]);
      std::uint8_t hi = (byte >> 4U) & 0xFU;
      std::uint8_t lo = byte & 0xFU;

      FormattedValue v_hi;
      if (!read_value(hi, v_hi))
        return false;
      values.push_back(v_hi);

      FormattedValue v_lo;
      if (!read_value(lo, v_lo))
        return false;
      values.push_back(v_lo);
    }
    return false;
  }

  bool read_value(std::uint8_t tc, FormattedValue& out) {
    out.tc = tc;

    if (tc <= tc_uint64) {
      std::size_t n = tc + 1;
      if (pos_ + n > size_)
        return false;
      out.u64 = 0;
      std::memcpy(&out.u64, data_ + pos_, n);
      pos_ += n;
      return true;
    }
    if (tc == tc_float) {
      if (pos_ + sizeof(float) > size_)
        return false;
      std::memcpy(&out.f32, data_ + pos_, sizeof(float));
      pos_ += sizeof(float);
      return true;
    }
    if (tc == tc_double) {
      if (pos_ + sizeof(double) > size_)
        return false;
      std::memcpy(&out.f64, data_ + pos_, sizeof(double));
      pos_ += sizeof(double);
      return true;
    }
    if (tc == tc_string8) {
      if (pos_ >= size_)
        return false;
      auto len = static_cast<std::size_t>(static_cast<std::uint8_t>(data_[pos_++]));
      if (pos_ + len > size_)
        return false;
      out.str = std::string_view(reinterpret_cast<const char*>(data_ + pos_), len);
      pos_ += len;
      return true;
    }
    if (tc == tc_string16) {
      if (pos_ + 2 > size_)
        return false;
      std::uint16_t len16 = 0;
      std::memcpy(&len16, data_ + pos_, 2);
      pos_ += 2;
      auto len = static_cast<std::size_t>(len16);
      if (pos_ + len > size_)
        return false;
      out.str = std::string_view(reinterpret_cast<const char*>(data_ + pos_), len);
      pos_ += len;
      return true;
    }
    return false;
  }

  template <typename SinkT>
  static void write_str(Buffer<SinkT>& lb, SinkT& sink, std::string_view s) {
    sink.write(&lb, reinterpret_cast<const std::byte*>(s.data()), s.size());
  }

  template <typename SinkT>
  static void write_uint(Buffer<SinkT>& lb, SinkT& sink, std::uint64_t val) {
    char buf[20];
    auto* end = buf + sizeof(buf);
    auto* p = end;
    if (val == 0) {
      *--p = '0';
    } else {
      while (val > 0) {
        *--p = static_cast<char>('0' + val % 10);
        val /= 10;
      }
    }
    sink.write(&lb, reinterpret_cast<const std::byte*>(p), static_cast<std::size_t>(end - p));
  }

  template <typename SinkT>
  static void write_float(Buffer<SinkT>& lb, SinkT& sink, double val) {
    char buf[32];
    auto len = std::snprintf(buf, sizeof(buf), "%g", val);
    if (len > 0) {
      sink.write(&lb, reinterpret_cast<const std::byte*>(buf), static_cast<std::size_t>(len));
    }
  }

  template <typename SinkT>
  static void write_value(Buffer<SinkT>& lb, SinkT& sink, const FormattedValue& v) {
    if (v.tc <= tc_uint64) {
      write_uint(lb, sink, v.u64);
    } else if (v.tc == tc_float) {
      write_float(lb, sink, static_cast<double>(v.f32));
    } else if (v.tc == tc_double) {
      write_float(lb, sink, v.f64);
    } else if (v.tc == tc_string8 || v.tc == tc_string16) {
      write_str(lb, sink, v.str);
    }
  }
};

}  // namespace bilog
