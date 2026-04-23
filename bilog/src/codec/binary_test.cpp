#include "bilog/codec/binary.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bilog/event.hpp"
#include "bilog/level.hpp"
#include "bilog/sink/ringbuff.hpp"
#include "bilog/tag.hpp"
#include "test_utils.hpp"

namespace {

using sink_t = bilog::RingBuffSink;
using buf_t = bilog::Buffer<sink_t>;
using event_t = bilog::Event<bilog::BinaryEncoder, sink_t>;

}  // namespace

// Header layout per nibble: [neg:1][width-1:3]
// Tag(1, "msg") has width=1, neg=0 → hi nibble is always 0x0 below.

TEST(BinaryEncoder, Uint) {
  auto check = [&](const char* name, auto val, int lo, std::initializer_list<int> payload) {
    SCOPED_TRACE(name);
    bilog::BinaryEncoder encoder;
    sink_t sink;
    buf_t buf(sink_t::kBuffCap);
    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), val);
    auto d = bilog::test::drain(&buf, &sink);

    ASSERT_EQ(d.size(), 2U + payload.size());
    EXPECT_EQ(d[0], static_cast<std::byte>(lo));
    EXPECT_EQ(d[1], static_cast<std::byte>(1));
    std::size_t i = 0;
    for (auto b : payload) {
      EXPECT_EQ(d[2 + i], static_cast<std::byte>(b));
      ++i;
    }
  };
  check("u8 zero", std::uint8_t{0}, 0x00, {0});
  check("u8 mid", std::uint8_t{42}, 0x00, {42});
  check("u8 max", std::numeric_limits<std::uint8_t>::max(), 0x00, {0xFF});
  check("u16 low high byte set", std::uint16_t{0x0100}, 0x01, {0x00, 0x01});
  check("u16 max", std::numeric_limits<std::uint16_t>::max(), 0x01, {0xFF, 0xFF});
  check("u32 high byte set", std::uint32_t{0x01000000}, 0x03, {0x00, 0x00, 0x00, 0x01});
  check("u32 max", std::numeric_limits<std::uint32_t>::max(), 0x03, {0xFF, 0xFF, 0xFF, 0xFF});
  check("u64 high byte set",
        std::uint64_t{0x0100000000000000},
        0x07,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
  check("u64 max",
        std::numeric_limits<std::uint64_t>::max(),
        0x07,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

TEST(BinaryEncoder, Int) {
  auto check = [&](const char* name, auto val, int lo, std::initializer_list<int> payload) {
    SCOPED_TRACE(name);
    bilog::BinaryEncoder encoder;
    sink_t sink;
    buf_t buf(sink_t::kBuffCap);

    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), val);
    auto d = bilog::test::drain(&buf, &sink);

    ASSERT_EQ(d.size(), 2U + payload.size());
    EXPECT_EQ(d[0], static_cast<std::byte>(lo));
    EXPECT_EQ(d[1], static_cast<std::byte>(1));
    std::size_t i = 0;
    for (auto b : payload) {
      EXPECT_EQ(d[2 + i], static_cast<std::byte>(b));
      ++i;
    }
  };
  check("i32 one", std::int32_t{1}, 0x00, {1});
  check("i32 positive mid", std::int32_t{42}, 0x00, {42});
  check("i32 max", std::numeric_limits<std::int32_t>::max(), 0x03, {0xFF, 0xFF, 0xFF, 0x7F});
  check("i64 max",
        std::numeric_limits<std::int64_t>::max(),
        0x07,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F});
  check("i32 -1 zigzag", std::int32_t{-1}, 0x08, {1});
  check("i32 -42 zigzag", std::int32_t{-42}, 0x08, {83});
  check("i32 min", std::numeric_limits<std::int32_t>::min(), 0x0B, {0xFF, 0xFF, 0xFF, 0xFF});
  check("i64 min",
        std::numeric_limits<std::int64_t>::min(),
        0x0F,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

TEST(BinaryEncoder, Float) {
  auto check = [&](const char* name, auto val, int lo) {
    SCOPED_TRACE(name);
    bilog::BinaryEncoder encoder;
    sink_t sink;
    buf_t buf(sink_t::kBuffCap);

    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), val);
    auto d = bilog::test::drain(&buf, &sink);

    ASSERT_EQ(d.size(), 2U + sizeof(val));
    EXPECT_EQ(d[0], static_cast<std::byte>(lo));
    EXPECT_EQ(d[1], static_cast<std::byte>(1));

    decltype(val) decoded = 0;
    std::memcpy(&decoded, d.data() + 2, sizeof(val));
    EXPECT_EQ(decoded, val);
  };
  check("float", 73.5F, 0x03);
  check("double", 12345.6789, 0x07);
}

TEST(BinaryEncoder, String) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), std::string("hello"));

  auto d = bilog::test::drain(&buf, &sink);
  ASSERT_EQ(d.size(), 8U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x00));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));
  EXPECT_EQ(d[2], static_cast<std::byte>(5));
  EXPECT_EQ(std::memcmp(d.data() + 3, "hello", 5), 0);
}

TEST(BinaryEncoder, Tag) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), bilog::Tag(2, "b"));

  auto d = bilog::test::drain(&buf, &sink);
  ASSERT_EQ(d.size(), 3U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x00));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));
  EXPECT_EQ(d[2], static_cast<std::byte>(2));
}

TEST(BinaryFormatter, Types) {
  auto check = [&](const char* name, auto val, bilog::FieldType type, std::string_view expected) {
    SCOPED_TRACE(name);
    bilog::BinaryEncoder encoder;
    sink_t sink;
    buf_t buf(sink_t::kBuffCap);

    encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
    encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg,"), std::uint8_t{0});
    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "val:"), val);
    encoder.commit(&buf, &sink);

    auto bin = bilog::test::drain(&buf, &sink);

    std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg,"}, {1, "val:"}};
    std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {{0, {type}}};

    std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
    bilog::BinaryFormatter fmt(&in);
    sink_t out;
    buf_t out_buf(sink_t::kBuffCap);
    ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
    EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), expected);
  };
  check("int positive", 42, bilog::FieldType::Int, "[INFO] msg, val: 42\n");
  check("int negative", -42, bilog::FieldType::Int, "[INFO] msg, val: -42\n");
  check("int min",
        std::numeric_limits<std::int64_t>::min(),
        bilog::FieldType::Int,
        "[INFO] msg, val: -9223372036854775808\n");
  check("int max",
        std::numeric_limits<std::int64_t>::max(),
        bilog::FieldType::Int,
        "[INFO] msg, val: 9223372036854775807\n");
  check("bool", true, bilog::FieldType::Bool, "[INFO] msg, val: true\n");
  check("float", 75.3F, bilog::FieldType::Float, "[INFO] msg, val: 75.3\n");
  check("string", "some string", bilog::FieldType::String, "[INFO] msg, val: some string\n");
}

TEST(BinaryFormatter, MultipleRecords) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kError.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(10, "shutdown"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(11, "code:"), std::uint8_t{7});
  encoder.commit(&buf, &sink);

  encoder.encode_pair(&buf, &sink, std::uint64_t{1}, bilog::Level::kTrace.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(20, "heartbeat"), std::uint8_t{0});
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);

  std::unordered_map<std::uint64_t, std::string> tags = {
      {10, "shutdown"}, {11, "code:"}, {20, "heartbeat"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {
      {0, {bilog::FieldType::Int}}, {1, {}}};

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);

  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_FALSE(fmt.format(&out_buf, &out, tags, events));

  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out),
            "[ERROR] shutdown code: 7\n[TRACE] heartbeat\n");
}

TEST(BinaryFormatter, NoSchema) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{5}, bilog::Level::kDebug.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(99, "msg"), std::uint8_t{0});
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);

  std::unordered_map<std::uint64_t, std::string> tags;
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events;

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[DEBUG] 99\n");
}

TEST(BinaryEncoder, LongString) {
  // String >255 bytes forces the 2-byte length-prefix path (wb == 2 in encode_pair).
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  std::string long_str(300, 'x');
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), long_str);
  auto d = bilog::test::drain(&buf, &sink);

  // Header byte: hi nibble = code(neg_a=0, width_a=1) = 0x0.
  //              lo nibble = code(neg_b=0, width_b=2) = 0x1.
  // Total: 1 (header) + 1 (tag id) + 2 (len16 = 300 = 0x012C little-endian) + 300 = 304.
  ASSERT_EQ(d.size(), 304U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x01));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));
  EXPECT_EQ(d[2], static_cast<std::byte>(0x2C));
  EXPECT_EQ(d[3], static_cast<std::byte>(0x01));
}

TEST(BinaryFormatter, ZeroInt) {
  // write_uint has a special branch for val == 0.
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "n:"), std::uint64_t{0});
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);
  std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg"}, {1, "n:"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {
      {0, {bilog::FieldType::Int}}};

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[INFO] msg n: 0\n");
}

TEST(BinaryFormatter, CStr) {
  // FieldType::CStr path in format_scalar: value.u64 is a tag ID, resolved via tag_names.
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "node:"), bilog::Tag(2, "stage1"));
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);
  std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg"}, {1, "node:"}, {2, "stage1"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {
      {0, {bilog::FieldType::CStr}}};

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[INFO] msg node: stage1\n");
}

TEST(BinaryFormatter, InvalidLevel) {
  // If Level::from_byte returns nullopt, the formatter still writes the
  // brackets with no name inside.
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, static_cast<std::byte>(99));
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);
  std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {{0, {}}};

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[] msg\n");
}

TEST(BinaryFormatter, DoubleAndFalse) {
  // Double (width=8) takes the non-float branch in format_scalar; bool false
  // takes the false side of the ternary.
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "f:"), 3.14);
  encoder.encode_pair(&buf, &sink, bilog::Tag(2, "b:"), false);
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);
  std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg"}, {1, "f:"}, {2, "b:"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {
      {0, {bilog::FieldType::Float, bilog::FieldType::Bool}}};

  std::istringstream in(std::string(reinterpret_cast<const char*>(bin.data()), bin.size()));
  bilog::BinaryFormatter fmt(&in);
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(&out_buf, &out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[INFO] msg f: 3.14 b: false\n");
}

TEST(BinaryFormatter, TruncatedStreams) {
  // Build a well-formed two-field record once, then feed prefixes of it to the
  // formatter. Every truncation point must be detected as format() == false
  // rather than writing garbage or crashing.
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "n:"), std::uint64_t{42});
  encoder.encode_pair(&buf, &sink, bilog::Tag(2, "s:"), std::string("hello"));
  encoder.commit(&buf, &sink);

  auto bin = bilog::test::drain(&buf, &sink);
  std::string full(reinterpret_cast<const char*>(bin.data()), bin.size());

  std::unordered_map<std::uint64_t, std::string> tags = {{0, "msg"}, {1, "n:"}, {2, "s:"}};
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> events = {
      {0, {bilog::FieldType::Int, bilog::FieldType::String}}};

  auto check = [&](const char* name, std::size_t prefix_len) {
    SCOPED_TRACE(name);
    std::istringstream in(full.substr(0, prefix_len));
    bilog::BinaryFormatter fmt(&in);
    sink_t out;
    buf_t out_buf(sink_t::kBuffCap);
    EXPECT_FALSE(fmt.format(&out_buf, &out, tags, events));
  };
  check("empty stream (eof at pair 0 header)", 0);
  check("pair 0 header only", 1);
  check("pair 0 partial payload", 2);
  check("pair 1 truncated", 4);
  check("dynamic int field header only", 7);
  check("dynamic string field header+len, payload missing", 12);
}

TEST(MinLevel, Allows) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  {
    event_t event(bilog::Level::kInfo, &encoder, &buf, &sink, {0, 1, 2});
    event.info("msg").i("val:", std::uint8_t{42}).write();
    sink.flush(&buf);
    EXPECT_GT(sink.available(), 0U);
    sink.clear();
  }
  {
    event_t event(bilog::Level::kInfo, &encoder, &buf, &sink, {0, 1, 2});
    event.error("msg").i("val:", std::uint8_t{42}).write();
    sink.flush(&buf);
    EXPECT_GT(sink.available(), 0U);
    sink.clear();
  }
}
