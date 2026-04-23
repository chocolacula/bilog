#include "bilog/codec/text.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "bilog/event.hpp"
#include "bilog/sink/ringbuff.hpp"
#include "test_utils.hpp"

namespace {

using sink_t = bilog::RingBuffSink;
using buf_t = bilog::Buffer<sink_t>;
using event_t = bilog::Event<bilog::TextEncoder, sink_t>;

}  // namespace

TEST(TextEncoder, AllLevels) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  auto test_level = [&](auto level_fn, const char* expected_tag) {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1});
    level_fn(event).write();

    auto output = bilog::test::drain_str(&buf, &sink);
    EXPECT_TRUE(output.starts_with(expected_tag));
    sink.clear();
  };

  test_level([](event_t& e) { return e.trace("msg"); }, "[TRACE] ");
  test_level([](event_t& e) { return e.debug("msg"); }, "[DEBUG] ");
  test_level([](event_t& e) { return e.info("msg"); }, "[INFO] ");
  test_level([](event_t& e) { return e.warn("msg"); }, "[WARN] ");
  test_level([](event_t& e) { return e.error("msg"); }, "[ERROR] ");
  test_level([](event_t& e) { return e.fatal("msg"); }, "[FATAL] ");
}

TEST(TextEncoder, InvalidLevelByte) {
  // Level::from_byte returns nullopt for bytes > kFatal. The encoder still
  // writes the brackets, just with an empty name.
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  encoder.encode_pair(&buf, &sink, std::uint64_t{0}, static_cast<std::byte>(99));
  encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
  encoder.commit(&buf, &sink);

  EXPECT_EQ(bilog::test::drain_str(&buf, &sink), "[] msg\n");
}

TEST(TextEncoder, EncodePair) {
  auto check = [](const char* name, const auto& val, std::string_view expected) {
    SCOPED_TRACE(name);
    bilog::TextEncoder encoder;
    sink_t sink;
    buf_t buf(sink_t::kBuffCap);

    encoder.encode_pair(&buf, &sink, std::uint64_t{0}, bilog::Level::kInfo.to_byte());
    encoder.encode_pair(&buf, &sink, bilog::Tag(0, "msg"), std::uint8_t{0});
    encoder.encode_pair(&buf, &sink, bilog::Tag(1, "v:"), val);
    encoder.commit(&buf, &sink);

    EXPECT_EQ(bilog::test::drain_str(&buf, &sink), expected);
  };
  check("tag", bilog::Tag(2, "stage1"), "[INFO] msg v: stage1\n");
  check("byte", std::byte{42}, "[INFO] msg v: 42\n");
  check("u8", std::uint8_t{42}, "[INFO] msg v: 42\n");
  check("u32", std::uint32_t{8080}, "[INFO] msg v: 8080\n");
  check("i64 positive", std::int64_t{123}, "[INFO] msg v: 123\n");
  check("i64 negative", std::int64_t{-456}, "[INFO] msg v: -456\n");
  check("bool true", true, "[INFO] msg v: true\n");
  check("bool false", false, "[INFO] msg v: false\n");
  check("float", 73.5F, "[INFO] msg v: 73.5\n");
  check("double", 3.14, "[INFO] msg v: 3.14\n");
  check("string", std::string("hello world"), "[INFO] msg v: hello world\n");
  check("c-string", "literal", "[INFO] msg v: literal\n");
}

TEST(TextEncoder, MultipleRecords) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.info("first").i("n:", 1).write();
  }
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.error("second").i("n:", 2).write();
  }
  EXPECT_EQ(bilog::test::drain_str(&buf, &sink), "[INFO] first n: 1\n[ERROR] second n: 2\n");
}
