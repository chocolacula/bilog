#include "bilog/sink/ringbuff.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include "test_utils.hpp"

namespace {
using buf_t = bilog::Buffer<bilog::RingBuffSink>;
}

TEST(RingBuffSink, WriteAndRead) {
  bilog::RingBuffSink sink(128);
  buf_t buf(bilog::RingBuffSink::kBuffCap);
  std::string_view msg = "hello";
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  sink.flush(&buf);

  EXPECT_EQ(sink.available(), 5U);
  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "hello");
  EXPECT_EQ(sink.available(), 0U);
}

TEST(RingBuffSink, WriteByte) {
  bilog::RingBuffSink sink(128);
  buf_t buf(bilog::RingBuffSink::kBuffCap);
  sink.write_byte(&buf, static_cast<std::byte>('A'));
  sink.write_byte(&buf, static_cast<std::byte>('B'));
  sink.flush(&buf);

  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "AB");
}

TEST(RingBuffSink, Clear) {
  bilog::RingBuffSink sink(128);
  buf_t buf(bilog::RingBuffSink::kBuffCap);
  std::string_view msg = "data";
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  sink.flush(&buf);

  sink.clear();
  EXPECT_EQ(sink.available(), 0U);
  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "");
}

TEST(RingBuffSink, Overflow) {
  bilog::RingBuffSink sink(8);
  buf_t buf(bilog::RingBuffSink::kBuffCap);
  std::string_view msg = "ABCDEFGHIJKL";  // 12 chars, buffer is 8
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  sink.flush(&buf);

  EXPECT_EQ(sink.available(), 8U);
  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "EFGHIJKL");
}

TEST(RingBuffSink, ReadPartial) {
  bilog::RingBuffSink sink(128);
  buf_t buf(bilog::RingBuffSink::kBuffCap);
  std::string_view msg = "hello world";
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  sink.flush(&buf);

  std::byte out[5];
  auto n = sink.read(out, 5);
  EXPECT_EQ(n, 5U);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(out), n), "hello");
  EXPECT_EQ(sink.available(), 6U);
  EXPECT_EQ(bilog::test::drain_str(&buf, sink), " world");
}

TEST(RingBuffSink, WrapAround) {
  bilog::RingBuffSink sink(16);
  buf_t buf(bilog::RingBuffSink::kBuffCap);

  std::string_view first = "AAAAAAAAAAAA";  // 12 bytes
  sink.write(&buf, reinterpret_cast<const std::byte*>(first.data()), first.size());
  sink.flush(&buf);
  sink.clear();

  std::string_view second = "BBBBBBBB";
  sink.write(&buf, reinterpret_cast<const std::byte*>(second.data()), second.size());
  sink.flush(&buf);

  EXPECT_EQ(bilog::test::drain_str(&buf, sink), "BBBBBBBB");
}

// --- Multithreaded tests ---

TEST(RingBuffSink, MultithreadedWrites) {
  constexpr int kThreads = 4;
  constexpr int kLinesPerThread = 1000;
  bilog::RingBuffSink sink(256 * 1024);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&sink, t] {
      buf_t buf(bilog::RingBuffSink::kBuffCap);
      for (int i = 0; i < kLinesPerThread; ++i) {
        auto line = "T" + std::to_string(t) + ":" + std::to_string(i) + "\n";
        sink.write(&buf, reinterpret_cast<const std::byte*>(line.data()), line.size());
        sink.commit(&buf);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  buf_t buf(bilog::RingBuffSink::kBuffCap);
  auto content = bilog::test::drain_str(&buf, sink);
  auto line_count = std::ranges::count(content, '\n');
  EXPECT_EQ(line_count, kThreads * kLinesPerThread);

  for (int t = 0; t < kThreads; ++t) {
    for (int i = 0; i < kLinesPerThread; ++i) {
      auto expected = "T" + std::to_string(t) + ":" + std::to_string(i) + "\n";
      EXPECT_NE(content.find(expected), std::string::npos);
    }
  }
}

TEST(RingBuffSink, NoInterleaving) {
  constexpr int kThreads = 4;
  constexpr int kWrites = 500;
  bilog::RingBuffSink sink(256 * 1024);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&sink, t] {
      buf_t buf(bilog::RingBuffSink::kBuffCap);
      char marker = static_cast<char>('A' + t);
      std::string chunk(64, marker);
      chunk += '\n';
      for (int i = 0; i < kWrites; ++i) {
        sink.write(&buf, reinterpret_cast<const std::byte*>(chunk.data()), chunk.size());
        sink.commit(&buf);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  buf_t buf(bilog::RingBuffSink::kBuffCap);
  auto content = bilog::test::drain_str(&buf, sink);

  std::size_t pos = 0;
  int lines = 0;
  while (pos < content.size()) {
    auto nl = content.find('\n', pos);
    if (nl == std::string::npos)
      break;

    auto line = content.substr(pos, nl - pos);
    ASSERT_EQ(line.size(), 64U) << "Line " << lines << " wrong length";

    char first = line[0];
    EXPECT_TRUE(first >= 'A' && first < 'A' + kThreads) << "Unexpected marker: " << first;
    EXPECT_EQ(line, std::string(64, first)) << "Interleaved data at line " << lines;

    pos = nl + 1;
    ++lines;
  }

  EXPECT_EQ(lines, kThreads * kWrites);
}

TEST(RingBuffSink, ThreadFlushOnExit) {
  constexpr int kThreads = 4;
  bilog::RingBuffSink sink(4096);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&sink, t] {
      buf_t buf(bilog::RingBuffSink::kBuffCap);
      auto msg = "thread" + std::to_string(t);
      sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
      sink.flush(&buf);
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  buf_t buf(bilog::RingBuffSink::kBuffCap);
  auto content = bilog::test::drain_str(&buf, sink);
  for (int t = 0; t < kThreads; ++t) {
    auto expected = "thread" + std::to_string(t);
    EXPECT_NE(content.find(expected), std::string::npos)
        << "Thread " << t << " buffer not flushed on exit";
  }
}
