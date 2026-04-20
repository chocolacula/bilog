#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bilog/level.hpp"
#include "bilog/sink/buffer.hpp"
#include "bilog/tag.hpp"

namespace bilog {

/// @brief Encodes values into a paired binary format.
///
/// Per-value header (4 bits): [neg:1][width-1:3]
///   neg   = 1 if the value was negative and zigzag-encoded.
///   width = number of payload bytes, 1..8 (encoded as 0..7).
///
/// Two values share one header byte: [hi:4][lo:4].
///
/// Dynamic strings still live in the same header: the "width" bytes encode
/// the string length, followed by that many payload bytes. The decoder
/// needs the schema to know a field is a string (same for float/bool).
///
/// A record is a sequence of pairs; its length is implicit from the schema
/// (event_id -> N dynamic fields). No terminator byte.
class BinaryEncoder {
 public:
  BinaryEncoder() = default;

  /// Encode a pair of fixed-size values.
  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> && !std::convertible_to<B, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    std::byte buf[17];
    std::uint64_t ua = 0;
    std::uint8_t wa = 0;
    std::uint8_t na = 0;
    prepare(a, ua, wa, na);

    std::uint64_t ub = 0;
    std::uint8_t wb = 0;
    std::uint8_t nb = 0;
    prepare(b, ub, wb, nb);

    buf[0] = make_header(na, wa, nb, wb);
    std::size_t pos = 1;
    write_payload(buf, &pos, ua, wa);
    write_payload(buf, &pos, ub, wb);
    sink->write(lb, buf, pos);
  }

  /// Encode a pair where the second value is a string.
  template <typename SinkT, typename A, typename B>
    requires(!std::convertible_to<A, std::string_view> && std::convertible_to<B, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    auto sv = std::string_view(b);
    auto len = std::min(sv.size(), static_cast<std::size_t>(0xFFFF));

    std::uint64_t ua = 0;
    std::uint8_t wa = 0;
    std::uint8_t na = 0;
    prepare(a, ua, wa, na);

    // String width = 1 if len fits in 1 byte, else 2.
    std::uint8_t wb = (len <= 0xFF) ? 1 : 2;

    std::byte buf[12];
    buf[0] = make_header(na, wa, 0, wb);

    std::size_t pos = 1;
    write_payload(buf, &pos, ua, wa);

    if (wb == 1) {
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
    sink->commit(lb);
  }

 private:
  /// Extract the encoded width (in bytes, 1..8) for an unsigned value.
  static std::uint8_t width_for(std::uint64_t v) {
    if (v == 0) {
      return 1;
    }
    auto bits = 64 - std::countl_zero(v);
    return static_cast<std::uint8_t>((bits + 7) / 8);
  }

  /// Pack (neg, width) into the 4-bit header code.
  static std::uint8_t code(std::uint8_t neg, std::uint8_t width) {
    auto w = static_cast<std::uint8_t>((width - 1U) & 0x7U);
    return static_cast<std::uint8_t>((neg << 3U) | w);
  }

  static std::byte make_header(std::uint8_t na, std::uint8_t wa, std::uint8_t nb, std::uint8_t wb) {
    auto hi = static_cast<std::uint8_t>(code(na, wa) << 4U);
    auto lo = code(nb, wb);
    return static_cast<std::byte>(hi | lo);
  }

  static void write_payload(std::byte* buf, std::size_t* pos, std::uint64_t v, std::uint8_t width) {
    std::memcpy(buf + *pos, &v, width);
    *pos += width;
  }

  // --- prepare: convert a C++ value into (unsigned payload, width, neg) ---

  static void prepare(const Tag& tag, std::uint64_t& u, std::uint8_t& w, std::uint8_t& n) {
    u = tag.id();
    w = width_for(u);
    n = 0;
  }

  static void prepare(std::byte b, std::uint64_t& u, std::uint8_t& w, std::uint8_t& n) {
    u = static_cast<std::uint8_t>(b);
    w = width_for(u);
    n = 0;
  }

  template <std::integral T>
  static void prepare(T val, std::uint64_t& u, std::uint8_t& w, std::uint8_t& n) {
    if constexpr (std::is_signed_v<T>) {
      if (val < 0) {
        // Zigzag: (n << 1) ^ (n >> (bits-1))
        auto s = static_cast<std::int64_t>(val);
        u = (static_cast<std::uint64_t>(s) << 1U) ^ static_cast<std::uint64_t>(s >> 63);
        w = width_for(u);
        n = 1;
        return;
      }
      u = static_cast<std::uint64_t>(val);
    } else {
      u = static_cast<std::uint64_t>(val);
    }
    w = width_for(u);
    n = 0;
  }

  template <std::floating_point T>
  static void prepare(T val, std::uint64_t& u, std::uint8_t& w, std::uint8_t& n) {
    u = 0;
    if constexpr (std::same_as<T, float>) {
      std::memcpy(&u, &val, sizeof(float));
      w = 4;
    } else {
      std::memcpy(&u, &val, sizeof(double));
      w = 8;
    }
    n = 0;
  }
};

/// Semantic type tag for a dynamic field, stored in the schema.
enum class FieldType : std::uint8_t {
  Int,     // "i" — signed/unsigned integer
  Float,   // "f" — float (width 4) or double (width 8)
  Bool,    // "b" — 1 byte, 0/1
  String,  // "s" — length-prefixed bytes
  CStr,    // "cs" — tag ID, rendered via tag_names
};

inline bool parse_field_type(std::string_view s, FieldType& out) {
  if (s == "i") {
    out = FieldType::Int;
    return true;
  }
  if (s == "f") {
    out = FieldType::Float;
    return true;
  }
  if (s == "b") {
    out = FieldType::Bool;
    return true;
  }
  if (s == "s") {
    out = FieldType::String;
    return true;
  }
  if (s == "cs") {
    out = FieldType::CStr;
    return true;
  }
  return false;
}

/// @brief Reads binary log data and writes formatted text.
///
/// Record layout (fixed part, hardcoded):
///   [event_id, level_byte]    pair 0
///   [msg_tag, 0_placeholder]  pair 1
/// Then N dynamic fields, each as a pair:
///   [field_name_tag, value]
/// where value's type comes from the schema's event field-type list.
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

  /// Format one record.
  /// tag_names: tag_id -> string (e.g. 101 -> "startup")
  /// event_fields: event_id -> list of FieldType for the dynamic fields
  template <typename SinkT>
  bool format(Buffer<SinkT>& lb,
              SinkT& sink,
              const std::unordered_map<std::uint64_t, std::string>& tag_names,
              const std::unordered_map<std::uint64_t, std::vector<FieldType>>& event_fields) {
    if (pos_ >= size_) {
      return false;
    }

    // Pair 0: event_id (unsigned) + level (1 byte)
    DecodedValue ev;
    DecodedValue lv;
    if (!read_pair(ev, lv))
      return false;

    auto event_id = ev.u64;
    auto level_byte = static_cast<std::byte>(lv.u64);

    // Pair 1: msg_tag (unsigned) + placeholder (1 byte)
    DecodedValue mt;
    DecodedValue ph;
    (void)ph;
    if (!read_pair(mt, ph))
      return false;

    auto msg_tag_id = mt.u64;

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

    // Message tag name
    write_tag_name(lb, sink, msg_tag_id, tag_names);

    // Dynamic fields, driven by schema. Each field is a pair [name_tag, value].
    // For strings, the pair header still defines the widths of name_tag and
    // the string-length prefix; the string payload bytes follow the pair.
    auto ev_it = event_fields.find(event_id);
    if (ev_it != event_fields.end()) {
      for (auto ft : ev_it->second) {
        DecodedValue name_tag;
        DecodedValue value;
        if (!read_pair(name_tag, value))
          return false;

        sink.write_byte(&lb, static_cast<std::byte>(' '));
        write_tag_name(lb, sink, name_tag.u64, tag_names);
        sink.write_byte(&lb, static_cast<std::byte>(' '));

        if (ft == FieldType::String) {
          // value.u64 is the length (from the pair); payload follows inline.
          auto len = value.u64;
          if (pos_ + len > size_)
            return false;
          std::string_view sv(reinterpret_cast<const char*>(data_ + pos_), len);
          pos_ += len;
          write_str(lb, sink, sv);
        } else {
          format_scalar(lb, sink, ft, value, tag_names);
        }
      }
    }

    sink.write_byte(&lb, static_cast<std::byte>('\n'));
    return true;
  }

 private:
  struct DecodedValue {
    std::uint8_t neg = 0;
    std::uint8_t width = 0;
    std::uint64_t u64 = 0;
  };

  /// Read one pair (two fixed-width values sharing a header byte).
  bool read_pair(DecodedValue& a, DecodedValue& b) {
    if (pos_ >= size_)
      return false;
    auto hdr = static_cast<std::uint8_t>(data_[pos_++]);
    decode_nibble((hdr >> 4U) & 0xFU, a.neg, a.width);
    decode_nibble(hdr & 0xFU, b.neg, b.width);
    return read_fixed_payload(a) && read_fixed_payload(b);
  }

  bool read_fixed_payload(DecodedValue& out) {
    if (pos_ + out.width > size_)
      return false;
    out.u64 = 0;
    std::memcpy(&out.u64, data_ + pos_, out.width);
    pos_ += out.width;
    return true;
  }

  static void decode_nibble(std::uint8_t nib, std::uint8_t& neg, std::uint8_t& width) {
    neg = (nib >> 3U) & 0x1U;
    width = static_cast<std::uint8_t>((nib & 0x7U) + 1U);
  }

  template <typename SinkT>
  static void format_scalar(Buffer<SinkT>& lb,
                            SinkT& sink,
                            FieldType ft,
                            const DecodedValue& v,
                            const std::unordered_map<std::uint64_t, std::string>& tag_names) {
    switch (ft) {
      case FieldType::Int:
        if (v.neg != 0U) {
          auto z = v.u64;
          auto mask = static_cast<std::uint64_t>(0U) - (z & 1U);
          auto signed_val = static_cast<std::int64_t>((z >> 1U) ^ mask);
          write_int(lb, sink, signed_val);
        } else {
          write_uint(lb, sink, v.u64);
        }
        return;
      case FieldType::Float:
        if (v.width == 4) {
          float f = 0;
          std::memcpy(&f, &v.u64, sizeof(float));
          write_float(lb, sink, static_cast<double>(f));
        } else {
          double d = 0;
          std::memcpy(&d, &v.u64, sizeof(double));
          write_float(lb, sink, d);
        }
        return;
      case FieldType::Bool:
        write_str(lb, sink, v.u64 != 0U ? std::string_view("true") : std::string_view("false"));
        return;
      case FieldType::CStr:
        write_tag_name(lb, sink, v.u64, tag_names);
        return;
      case FieldType::String:
        return;  // handled by caller before this is called
    }
  }

  template <typename SinkT>
  static void write_tag_name(Buffer<SinkT>& lb,
                             SinkT& sink,
                             std::uint64_t id,
                             const std::unordered_map<std::uint64_t, std::string>& tag_names) {
    auto it = tag_names.find(id);
    if (it != tag_names.end()) {
      write_str(lb, sink, it->second);
    } else {
      write_uint(lb, sink, id);
    }
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
  static void write_int(Buffer<SinkT>& lb, SinkT& sink, std::int64_t val) {
    if (val < 0) {
      sink.write_byte(&lb, static_cast<std::byte>('-'));
      // Careful with INT64_MIN — negate via unsigned.
      auto abs_val = static_cast<std::uint64_t>(-(val + 1)) + 1U;
      write_uint(lb, sink, abs_val);
    } else {
      write_uint(lb, sink, static_cast<std::uint64_t>(val));
    }
  }

  template <typename SinkT>
  static void write_float(Buffer<SinkT>& lb, SinkT& sink, double val) {
    char buf[32];
    auto len = std::snprintf(buf, sizeof(buf), "%g", val);
    if (len > 0) {
      sink.write(&lb, reinterpret_cast<const std::byte*>(buf), static_cast<std::size_t>(len));
    }
  }
};

}  // namespace bilog
