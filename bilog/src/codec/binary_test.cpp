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
  auto check = [&](auto val, int lo, std::initializer_list<int> payload) {
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
      EXPECT_EQ(d[2 + i], static_cast<std::byte>(b)) << "byte " << i;
      ++i;
    }
  };

  // width=1, neg=0 → lo=0x0
  check(std::uint8_t{0}, 0x00, {0});
  check(std::uint8_t{42}, 0x00, {42});
  check(std::numeric_limits<std::uint8_t>::max(), 0x00, {0xFF});

  // width=2, neg=0 → lo=0x1
  check(std::uint16_t{0x0100}, 0x01, {0x00, 0x01});
  check(std::numeric_limits<std::uint16_t>::max(), 0x01, {0xFF, 0xFF});

  // width=4, neg=0 → lo=0x3
  check(std::uint32_t{0x01000000}, 0x03, {0x00, 0x00, 0x00, 0x01});
  check(std::numeric_limits<std::uint32_t>::max(), 0x03, {0xFF, 0xFF, 0xFF, 0xFF});

  // width=8, neg=0 → lo=0x7
  check(std::uint64_t{0x0100000000000000}, 0x07, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
  check(std::numeric_limits<std::uint64_t>::max(),
        0x07,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

TEST(BinaryEncoder, Int) {
  auto check = [&](auto val, int lo, std::initializer_list<int> payload) {
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
      EXPECT_EQ(d[2 + i], static_cast<std::byte>(b)) << "byte " << i;
      ++i;
    }
  };

  // Positive: same layout as unsigned (neg=0).
  check(std::int32_t{1}, 0x00, {1});
  check(std::int32_t{42}, 0x00, {42});
  check(std::numeric_limits<std::int32_t>::max(), 0x03, {0xFF, 0xFF, 0xFF, 0x7F});
  check(std::numeric_limits<std::int64_t>::max(),
        0x07,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F});

  // Negative: zigzag-encoded, neg=1 → lo nibble gets 0x8 bit set.
  check(std::int32_t{-1}, 0x08, {1});    // zigzag(-1) = 1
  check(std::int32_t{-42}, 0x08, {83});  // zigzag(-42) = 83
  check(std::numeric_limits<std::int32_t>::min(), 0x0B, {0xFF, 0xFF, 0xFF, 0xFF});
  check(std::numeric_limits<std::int64_t>::min(),
        0x0F,
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

TEST(BinaryEncoder, Float) {
  auto check = [&](auto val, int lo) {
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

  check(73.5F, 0x03);       // float, width=4, neg=0
  check(12345.6789, 0x07);  // double, width=8, neg=0
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

// --- BinaryFormatter tests ---

TEST(BinaryFormatter, Types) {
  auto check = [&](auto val, bilog::FieldType type, std::string_view expected) {
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

  check(42, bilog::FieldType::Int, "[INFO] msg, val: 42\n");
  check(-42, bilog::FieldType::Int, "[INFO] msg, val: -42\n");
  check(std::numeric_limits<std::int64_t>::min(),
        bilog::FieldType::Int,
        "[INFO] msg, val: -9223372036854775808\n");
  check(std::numeric_limits<std::int64_t>::max(),
        bilog::FieldType::Int,
        "[INFO] msg, val: 9223372036854775807\n");

  check(true, bilog::FieldType::Bool, "[INFO] msg, val: true\n");
  check(75.3F, bilog::FieldType::Float, "[INFO] msg, val: 75.3\n");
  check("some string", bilog::FieldType::String, "[INFO] msg, val: some string\n");
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

  EXPECT_EQ(bilog::test::drain_str(&out_buf, &out), "[ERROR] shutdown code: 7\n[TRACE] heartbeat\n");
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

// --- MinLevel tests ---

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

TEST(MinLevel, Blocks) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  event_t event(bilog::Level::kError, &encoder, &buf, &sink, {0, 1, 2});
  event.info("msg").i("val:", std::uint8_t{42}).write();
  EXPECT_EQ(sink.available(), 0U);
}
