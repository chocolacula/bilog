#include "bilog/codec/binary.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
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

// --- BinaryEncoder raw byte tests ---

TEST(BinaryEncoder, Uint) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  {
    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), std::uint8_t{42});

    auto d = bilog::test::drain(&buf, sink);
    ASSERT_EQ(d.size(), 3U);
    EXPECT_EQ(d[0], static_cast<std::byte>(0x00));
    EXPECT_EQ(d[1], static_cast<std::byte>(1));
    EXPECT_EQ(d[2], static_cast<std::byte>(42));
  }
  {
    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), std::uint64_t{7458596});

    auto d = bilog::test::drain(&buf, sink);
    ASSERT_EQ(d.size(), 5U);
    EXPECT_EQ(d[0], static_cast<std::byte>(0x02));
    EXPECT_EQ(d[1], static_cast<std::byte>(1));
    EXPECT_EQ(d[2], static_cast<std::byte>(0x24));
    EXPECT_EQ(d[3], static_cast<std::byte>(0xCF));
    EXPECT_EQ(d[4], static_cast<std::byte>(0x71));
  }
}

TEST(BinaryEncoder, Float) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), 73.5F);

  auto d = bilog::test::drain(&buf, sink);
  ASSERT_EQ(d.size(), 6U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x08));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));

  float decoded = 0;
  std::memcpy(&decoded, d.data() + 2, sizeof(float));
  EXPECT_FLOAT_EQ(decoded, 73.5F);
}

TEST(BinaryEncoder, String) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), std::string("hello"));

  auto d = bilog::test::drain(&buf, sink);
  ASSERT_EQ(d.size(), 8U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x0C));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));
  EXPECT_EQ(d[2], static_cast<std::byte>(5));
  EXPECT_EQ(std::memcmp(d.data() + 3, "hello", 5), 0);
}

TEST(BinaryEncoder, Tag) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  encoder.encode_pair(&buf, &sink, bilog::Tag(1, "msg"), bilog::Tag(2, "b"));

  auto d = bilog::test::drain(&buf, sink);
  ASSERT_EQ(d.size(), 3U);
  EXPECT_EQ(d[0], static_cast<std::byte>(0x00));
  EXPECT_EQ(d[1], static_cast<std::byte>(1));
  EXPECT_EQ(d[2], static_cast<std::byte>(2));
}

// --- BinaryFormatter tests ---

TEST(BinaryFormatter, SingleRecord) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{11}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(101, "startup"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(3, "code:"), std::uint8_t{42});
  encoder.finish(&buf, &sink);

  auto bin = bilog::test::drain(&buf, sink);

  std::unordered_map<std::uint64_t, std::string> tags = {{101, "startup"}, {3, "code:"}};
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> events = {{11, {2, 4}}};

  bilog::BinaryFormatter fmt(bin.data(), bin.size());
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, out), "[INFO] startup code: 42\n");
}

TEST(BinaryFormatter, FloatField) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{12}, bilog::Level::kWarn.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(102, "temp"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(4, "celsius:"), 73.5F);
  encoder.finish(&buf, &sink);

  auto bin = bilog::test::drain(&buf, sink);

  std::unordered_map<std::uint64_t, std::string> tags = {{102, "temp"}, {4, "celsius:"}};
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> events = {{12, {2, 4}}};

  bilog::BinaryFormatter fmt(bin.data(), bin.size());
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, out), "[WARN] temp celsius: 73.5\n");
}

TEST(BinaryFormatter, MultipleRecords) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kError.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(10, "shutdown"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(11, "code:"), std::uint8_t{7});
  encoder.finish(&buf, &sink);

  encoder.encode_pair(&buf, &sink, std::uint64_t{1}, bilog::Level::kTrace.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(20, "heartbeat"), std::uint8_t{0});
  encoder.finish(&buf, &sink);

  auto bin = bilog::test::drain(&buf, sink);

  std::unordered_map<std::uint64_t, std::string> tags = {
      {10, "shutdown"}, {11, "code:"}, {20, "heartbeat"}};
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> events = {{0, {2, 4}}, {1, {2}}};

  bilog::BinaryFormatter fmt(bin.data(), bin.size());
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);

  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  EXPECT_FALSE(fmt.has_data());

  EXPECT_EQ(bilog::test::drain_str(&out_buf, out), "[ERROR] shutdown code: 7\n[TRACE] heartbeat\n");
}

TEST(BinaryFormatter, NoSchema) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{5}, bilog::Level::kDebug.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(99, "msg"), std::uint8_t{0});
  encoder.finish(&buf, &sink);

  auto bin = bilog::test::drain(&buf, sink);

  std::unordered_map<std::uint64_t, std::string> tags;
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> events;

  bilog::BinaryFormatter fmt(bin.data(), bin.size());
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, out), "[DEBUG] 99\n");
}

TEST(BinaryFormatter, StringField) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{3}, bilog::Level::kInfo.to_byte());
  encoder.encode_pair(&buf, &sink, bilog::Tag(50, "download"), std::uint8_t{0});
  encoder.encode_pair(&buf, &sink, bilog::Tag(51, "file:"), std::string("test.log"));
  encoder.finish(&buf, &sink);

  auto bin = bilog::test::drain(&buf, sink);

  std::unordered_map<std::uint64_t, std::string> tags = {{50, "download"}, {51, "file:"}};
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> events = {{3, {2, 4}}};

  bilog::BinaryFormatter fmt(bin.data(), bin.size());
  sink_t out;
  buf_t out_buf(sink_t::kBuffCap);
  ASSERT_TRUE(fmt.format(out_buf, out, tags, events));
  EXPECT_EQ(bilog::test::drain_str(&out_buf, out), "[INFO] download file: test.log\n");
}

// --- MinLevel tests ---

TEST(MinLevel, Allows) {
  bilog::BinaryEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  {
    event_t event(bilog::Level::kInfo, &encoder, &buf, &sink, {0, 1, 2});
    event.info("msg").num("val:", std::uint8_t{42}).write();
    sink.flush(&buf);
    EXPECT_GT(sink.available(), 0U);
    sink.clear();
  }
  {
    event_t event(bilog::Level::kInfo, &encoder, &buf, &sink, {0, 1, 2});
    event.error("msg").num("val:", std::uint8_t{42}).write();
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
  event.info("msg").num("val:", std::uint8_t{42}).write();
  EXPECT_EQ(sink.available(), 0U);
}
