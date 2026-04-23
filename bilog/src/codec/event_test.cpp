#include "bilog/event.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "bilog/codec/text.hpp"
#include "bilog/sink/ringbuff.hpp"
#include "test_utils.hpp"

namespace {

using sink_t = bilog::RingBuffSink;
using buf_t = bilog::Buffer<sink_t>;
using event_t = bilog::Event<bilog::TextEncoder, sink_t>;

}  // namespace

TEST(Event, LevelFilter) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  auto check = [&](const char* name, bilog::Level min_level, auto method) {
    SCOPED_TRACE(name);
    event_t event(min_level, &encoder, &buf, &sink, {0, 1});
    method(event).i("k:", 1U).write();
    EXPECT_TRUE(bilog::test::drain_str(&buf, &sink).empty());
    sink.clear();
  };

  auto trace = [](event_t& e) { return e.trace("msg"); };
  auto debug = [](event_t& e) { return e.debug("msg"); };
  auto info = [](event_t& e) { return e.info("msg"); };
  auto warn = [](event_t& e) { return e.warn("msg"); };
  auto error = [](event_t& e) { return e.error("msg"); };

  check("debug > trace", bilog::Level::kDebug, trace);
  check("info > debug", bilog::Level::kInfo, debug);
  check("warn > info", bilog::Level::kWarn, info);
  check("error > warn", bilog::Level::kError, warn);
  check("fatal > error", bilog::Level::kFatal, error);
}

TEST(Event, SuppressedChain) {
  // Covers the `event_ == nullptr` early-return in every EventWriter method:
  // .info() is filtered by min_level = kFatal, so subsequent chain calls run
  // against a null EventWriter and must be no-ops.
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  std::string s = "x";
  event_t event(bilog::Level::kFatal, &encoder, &buf, &sink, {0, 1});
  event.info("msg").i("a:", 1U).f("b:", 1.0F).b("c:", true).s("d:", s).cs("e:", "v").write();

  EXPECT_TRUE(bilog::test::drain_str(&buf, &sink).empty());
}

TEST(Event, ChainMethods) {
  // Each chain method (.i/.f/.b/.s/.cs) dispatches to a specific encode_pair
  // overload. Exercises the event-API path that `Types` used to cover.
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  auto check = [&](const char* name, auto chain, std::string_view expected) {
    SCOPED_TRACE(name);
    event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2, 3});
    chain(event).write();
    EXPECT_EQ(bilog::test::drain_str(&buf, &sink), expected);
    sink.clear();
  };

  std::string s = "hello world";
  check(
      "i uint32",
      [](event_t& e) { return e.info("startup").i("port:", std::uint32_t{8080}); },
      "[INFO] startup port: 8080\n");
  check(
      "bool",
      [](event_t& e) { return e.info("status").b("ok:", true); },
      "[INFO] status ok: true\n");
  check(
      "float",
      [](event_t& e) { return e.warn("temp").f("celsius:", 73.5F); },
      "[WARN] temp celsius: 73.5\n");
  check(
      "const string",
      [](event_t& e) { return e.info("download").cs("file:", "test.log"); },
      "[INFO] download file: test.log\n");
  check(
      "string",
      [&s](event_t& e) { return e.debug("msg").s("data:", s); },
      "[DEBUG] msg data: hello world\n");
}

TEST(Event, ChainTwoFields) {
  // Multi-field chain: exercises the "second and later pair" branch of
  // encode_pair (pair_index_ != 1) for both fields.
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  event_t event(bilog::Level::kTrace, &encoder, &buf, &sink, {0, 1, 2, 3});
  event.error("crash").i("code:", std::uint8_t{42}).i("pid:", std::uint32_t{12345}).write();
  EXPECT_EQ(bilog::test::drain_str(&buf, &sink), "[ERROR] crash code: 42 pid: 12345\n");
}

TEST(Event, LevelEquals) {
  bilog::TextEncoder encoder;
  sink_t sink;
  buf_t buf(sink_t::kBuffCap);

  event_t event(bilog::Level::kError, &encoder, &buf, &sink, {0, 1, 2});
  event.error("msg").i("val:", std::uint8_t{42}).write();
  EXPECT_FALSE(bilog::test::drain_str(&buf, &sink).empty());
}
