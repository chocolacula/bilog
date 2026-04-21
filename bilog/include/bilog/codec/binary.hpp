#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <istream>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
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
/// needs the schema to know a field type.
///
/// A record is a sequence of pairs, its length is implicit from the schema
/// event_id: N dynamic fields.
class BinaryEncoder {
 public:
  BinaryEncoder() = default;

  /// Encode a pair of fixed-size values.
  template <typename SinkT, typename ValT>
    requires(!std::convertible_to<ValT, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const Tag& tag, const ValT& val) {
    encode_pair(lb, sink, tag.id(), val);
  }

  template <typename SinkT, typename A, typename B>
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const A& a, const B& b) {
    auto [ua, width_a, neg_a] = prepare(a);
    auto [ub, width_b, neg_b] = prepare(b);

    sink->write_byte(lb, make_header(neg_a, width_a, neg_b, width_b));
    sink->write(lb, reinterpret_cast<const std::byte*>(&ua), width_a);
    sink->write(lb, reinterpret_cast<const std::byte*>(&ub), width_b);
  }

  /// Encode a dynamic string, if it's to long and length > uint16_t::max it will be truncated.
  template <typename SinkT, typename ValT>
    requires(std::convertible_to<ValT, std::string_view>)
  void encode_pair(Buffer<SinkT>* lb, SinkT* sink, const Tag& tag, const ValT& val) {
    auto sv = std::string_view(val);
    auto len = std::min(sv.size(), static_cast<std::size_t>(0xFFFF));

    auto [ua, wa, na] = prepare(tag);

    // String width = 1 if len fits in 1 byte, else 2.
    std::uint8_t wb = (len <= 0xFF) ? 1 : 2;

    sink->write_byte(lb, make_header(na, wa, 0, wb));
    sink->write(lb, reinterpret_cast<const std::byte*>(&ua), wa);

    auto len16 = static_cast<std::uint16_t>(len);
    sink->write(lb, reinterpret_cast<const std::byte*>(&len16), wb);
    sink->write(lb, reinterpret_cast<const std::byte*>(sv.data()), len);
  }

  template <typename SinkT>
  void commit(Buffer<SinkT>* lb, SinkT* sink) {
    sink->commit(lb);
  }

 private:
  /// Extract the encoded width (in bytes, 1..8) for an unsigned value.
  static std::uint8_t width(std::uint64_t v) {
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

  static std::byte make_header(std::uint8_t neg_a,
                               std::uint8_t width_a,
                               std::uint8_t neg_b,
                               std::uint8_t width_b) {
    auto hi = static_cast<std::uint8_t>(code(neg_a, width_a) << 4U);
    auto lo = code(neg_b, width_b);
    return static_cast<std::byte>(hi | lo);
  }

  // --- prepare: convert a C++ value into (unsigned payload, width, neg) ---

  using Prepared = std::tuple<std::uint64_t, std::uint8_t, std::uint8_t>;

  static Prepared prepare(const Tag& tag) {
    auto u = tag.id();
    return {u, width(u), 0};
  }

  static Prepared prepare(std::byte b) {
    auto u = static_cast<std::uint64_t>(static_cast<std::uint8_t>(b));
    return {u, width(u), 0};
  }

  template <std::integral T>
  static Prepared prepare(T val) {
    if constexpr (std::is_signed_v<T>) {
      if (val < 0) {
        // Zigzag: (n << 1) ^ (n >> (bits-1))
        auto s = static_cast<std::int64_t>(val);
        auto u = (static_cast<std::uint64_t>(s) << 1U) ^ static_cast<std::uint64_t>(s >> 63);
        return {u, width(u), 1};
      }
    }
    auto u = static_cast<std::uint64_t>(val);
    return {u, width(u), 0};
  }

  template <std::floating_point T>
  static Prepared prepare(T val) {
    std::uint64_t u = 0;
    if constexpr (std::same_as<T, float>) {
      std::memcpy(&u, &val, sizeof(float));
      return {u, 4, 0};
    } else {
      std::memcpy(&u, &val, sizeof(double));
      return {u, 8, 0};
    }
  }
};

/// Semantic type tag for a dynamic field, stored in the schema.
enum class FieldType : std::uint8_t {
  Int,    // signed/unsigned integer
  Float,  // float (width 4) or double (width 8)
  Bool,
  String,
  CStr,
};

/// @brief Reads binary log data and writes formatted text.
///
/// Record layout (fixed part, hardcoded):
///   [event_id, level_byte]    pair 0
///   [msg_tag, 0_placeholder]  pair 1
/// Then N dynamic fields, each as a pair:
///   [field_name_tag, value]
/// where value's type comes from the schema's event field-type list.
class BinaryFormatter {
  std::istream* in_;

 public:
  explicit BinaryFormatter(std::istream* in) : in_(in) {}

  /// Format one record. Returns true if a record was written, false at clean
  /// EOF or on a mid-record read error.
  /// tag_names: tag_id -> string (e.g. 101 -> "startup")
  /// event_fields: event_id -> list of FieldType for the dynamic fields
  template <typename SinkT>
  bool format(Buffer<SinkT>* lb,
              SinkT* sink,
              const std::unordered_map<std::uint64_t, std::string>& tag_names,
              const std::unordered_map<std::uint64_t, std::vector<FieldType>>& event_fields) {
    // peek() sets eofbit if the stream is at EOF; use it to distinguish clean
    // end-of-stream from a read failure inside a record.
    if (in_->peek() == std::char_traits<char>::eof()) {
      return false;
    }

    // Pair 0: event_id (unsigned) + level (1 byte)
    DecodedValue ev;
    DecodedValue lv;
    if (!read_pair(&ev, &lv)) {
      return false;
    }
    auto event_id = ev.u64;
    auto level_byte = static_cast<std::byte>(lv.u64);

    // Pair 1: msg_tag (unsigned) + placeholder (1 byte)
    DecodedValue mt;
    DecodedValue ph;
    (void)ph;
    if (!read_pair(&mt, &ph)) {
      return false;
    }
    auto msg_tag_id = mt.u64;

    // Write: [LEVEL]
    sink->write_byte(lb, static_cast<std::byte>('['));
    auto lvl = Level::from_byte(level_byte);
    if (lvl) {
      auto name = lvl->to_str();
      if (name) {
        write_str(lb, sink, *name);
      }
    }
    sink->write_byte(lb, static_cast<std::byte>(']'));
    sink->write_byte(lb, static_cast<std::byte>(' '));

    // Message tag name
    write_tag(lb, sink, msg_tag_id, tag_names);

    // Dynamic fields, driven by schema. Each field is a pair [name_tag, value].
    // For strings, the pair header still defines the widths of name_tag and
    // the string-length prefix; the string payload bytes follow the pair.
    auto ev_it = event_fields.find(event_id);
    if (ev_it != event_fields.end()) {
      for (auto ft : ev_it->second) {
        DecodedValue name_tag;
        DecodedValue value;
        if (!read_pair(&name_tag, &value)) {
          return false;
        }
        sink->write_byte(lb, static_cast<std::byte>(' '));
        write_tag(lb, sink, name_tag.u64, tag_names);
        sink->write_byte(lb, static_cast<std::byte>(' '));

        if (ft == FieldType::String) {
          // value.u64 holds the string length; stream that many bytes through
          // a stack scratch buffer into the sink.
          if (!write_string_from_stream(lb, sink, value.u64)) {
            return false;
          }
        } else {
          format_scalar(lb, sink, ft, value, tag_names);
        }
      }
    }

    sink->write_byte(lb, static_cast<std::byte>('\n'));
    return true;
  }

 private:
  struct DecodedValue {
    std::uint8_t neg = 0;
    std::uint8_t width = 0;
    std::uint64_t u64 = 0;
  };

  /// Read one pair (two fixed-width values sharing a header byte).
  bool read_pair(DecodedValue* a, DecodedValue* b) {
    char hdr_ch = 0;
    if (!in_->read(&hdr_ch, 1)) {
      return false;
    }
    auto hdr = static_cast<std::uint8_t>(hdr_ch);
    decode_nibble((hdr >> 4U) & 0xFU, &a->neg, &a->width);
    decode_nibble(hdr & 0xFU, &b->neg, &b->width);
    return read_fixed_payload(a) && read_fixed_payload(b);
  }

  bool read_fixed_payload(DecodedValue* out) {
    out->u64 = 0;
    return static_cast<bool>(in_->read(reinterpret_cast<char*>(&out->u64), out->width));
  }

  /// Stream `len` bytes from the input into the sink via a stack scratch
  /// buffer. Avoids allocating for arbitrarily large strings.
  template <typename SinkT>
  bool write_string_from_stream(Buffer<SinkT>* lb, SinkT* sink, std::uint64_t len) {
    constexpr std::size_t kChunk = 256;
    char scratch[kChunk];
    while (len > 0) {
      auto n = static_cast<std::size_t>(std::min<std::uint64_t>(len, kChunk));
      if (!in_->read(scratch, static_cast<std::streamsize>(n))) {
        return false;
      }
      sink->write(lb, reinterpret_cast<const std::byte*>(scratch), n);
      len -= n;
    }
    return true;
  }

  static void decode_nibble(std::uint8_t nib, std::uint8_t* neg, std::uint8_t* width) {
    *neg = (nib >> 3U) & 0x1U;
    *width = static_cast<std::uint8_t>((nib & 0x7U) + 1U);
  }

  template <typename SinkT>
  static void format_scalar(Buffer<SinkT>* lb,
                            SinkT* sink,
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
        write_tag(lb, sink, v.u64, tag_names);
        return;
      case FieldType::String:
        return;  // handled by caller before this is called
    }
  }

  template <typename SinkT>
  static void write_tag(Buffer<SinkT>* lb,
                        SinkT* sink,
                        std::uint64_t id,
                        const std::unordered_map<std::uint64_t, std::string>& tags) {
    auto it = tags.find(id);
    if (it != tags.end()) {
      write_str(lb, sink, it->second);
    } else {
      write_uint(lb, sink, id);
    }
  }

  template <typename SinkT>
  static void write_str(Buffer<SinkT>* lb, SinkT* sink, std::string_view s) {
    sink->write(lb, reinterpret_cast<const std::byte*>(s.data()), s.size());
  }

  template <typename SinkT>
  static void write_uint(Buffer<SinkT>* lb, SinkT* sink, std::uint64_t val) {
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
    sink->write(lb, reinterpret_cast<const std::byte*>(p), static_cast<std::size_t>(end - p));
  }

  template <typename SinkT>
  static void write_int(Buffer<SinkT>* lb, SinkT* sink, std::int64_t val) {
    if (val < 0) {
      sink->write_byte(lb, static_cast<std::byte>('-'));
      // Careful with INT64_MIN — negate via unsigned.
      auto abs_val = static_cast<std::uint64_t>(-(val + 1)) + 1U;
      write_uint(lb, sink, abs_val);
    } else {
      write_uint(lb, sink, static_cast<std::uint64_t>(val));
    }
  }

  template <typename SinkT>
  static void write_float(Buffer<SinkT>* lb, SinkT* sink, double val) {
    char buf[32];
    auto len = std::snprintf(buf, sizeof(buf), "%g", val);
    if (len > 0) {
      sink->write(lb, reinterpret_cast<const std::byte*>(buf), static_cast<std::size_t>(len));
    }
  }
};

}  // namespace bilog
