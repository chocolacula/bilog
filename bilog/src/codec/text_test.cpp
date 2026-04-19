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

TEST(TextEncoder, Types) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.info("startup").num("port:", std::uint32_t{8080}).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[INFO] startup port: 8080\n");
  }
  sink.clear();
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.info("status").boo("ok:", true).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[INFO] status ok: 1\n");
  }
  sink.clear();
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.warn("temp").num("celsius:", 73.5F).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[WARN] temp celsius: 73.5\n");
  }
  sink.clear();
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2, 3});
    event.error("crash").num("code:", std::uint8_t{42}).num("pid:", std::uint32_t{12345}).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[ERROR] crash code: 42 pid: 12345\n");
  }
  sink.clear();
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2, 3});
    event.info("download").cstr("file:", "test.log").write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[INFO] download file: test.log\n");
  }
  sink.clear();
  {
    std::string value = "hello world";
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.debug("msg").str("data:", value).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[DEBUG] msg data: hello world\n");
  }
}

TEST(TextEncoder, AllLevels) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  auto test_level = [&](auto level_fn, const char* expected_tag) {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1});
    level_fn(event).write();

    auto output = bilog::test::drain_str(&buf, sink);
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

TEST(TextEncoder, MultipleRecords) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.info("first").num("n:", 1).write();
  }
  {
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2});
    event.error("second").num("n:", 2).write();
  }
  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "[INFO] first n: 1\n[ERROR] second n: 2\n");
}
