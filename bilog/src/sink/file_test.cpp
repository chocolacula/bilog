#include "bilog/sink/file.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

using buf_t = bilog::Buffer<bilog::FileSink>;

fs::path temp_filepath(const char* name) {
  return fs::temp_directory_path() / name;
}

std::string read_file(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return content;
}

void write_str(buf_t& buf, bilog::FileSink& sink, std::string_view sv) {
  sink.write(&buf, reinterpret_cast<const std::byte*>(sv.data()), sv.size());
}

}  // namespace

TEST(FileSink, Write) {
  auto path = temp_filepath("bilog_filesink.bin");
  {
    bilog::FileSink sink(path);
    buf_t buf(bilog::FileSink::kBuffCap);

    sink.write_byte(&buf, static_cast<std::byte>('A'));
    sink.write_byte(&buf, static_cast<std::byte>('B'));
    sink.write_byte(&buf, static_cast<std::byte>('C'));
    sink.write_byte(&buf, static_cast<std::byte>(' '));

    write_str(buf, sink, "hello world");
    sink.flush(&buf);
  }
  EXPECT_EQ(read_file(path), "ABC hello world");
  fs::remove(path);
}

TEST(FileSink, ManySmallWrites) {
  auto path = temp_filepath("bilog_filesink.bin");
  {
    bilog::FileSink sink(path);
    buf_t buf(bilog::FileSink::kBuffCap);
    for (int i = 0; i < 5000; ++i) {
      sink.write_byte(&buf, static_cast<std::byte>('.'));
    }
    sink.flush(&buf);
  }

  auto content = read_file(path);
  EXPECT_EQ(content, std::string(5000, '.'));
  fs::remove(path);
}

TEST(FileSink, MoveAssignment) {
  auto path1 = temp_filepath("bilog_filesink1.bin");
  auto path2 = temp_filepath("bilog_filesink2.bin");
  {
    bilog::FileSink sink(path1);
    buf_t buf(bilog::FileSink::kBuffCap);
    write_str(buf, sink, "first");

    sink.flush(&buf);
    sink = bilog::FileSink(path2);
    write_str(buf, sink, "second");
    sink.flush(&buf);
  }
  EXPECT_EQ(read_file(path1), "first");
  EXPECT_EQ(read_file(path2), "second");
  fs::remove(path1);
  fs::remove(path2);
}

TEST(FileSink, DefaultNoOp) {
  bilog::FileSink sink;
  buf_t buf(bilog::FileSink::kBuffCap);

  write_str(buf, sink, "[INFO] ");
  sink.write_byte(&buf, static_cast<std::byte>('1'));

  std::string full(4096, '1');
  write_str(buf, sink, full);
}

TEST(FileSink, MultithreadedWrites) {
  auto path = temp_filepath("bilog_filesink_mt.bin");
  constexpr int kThreads = 4;
  constexpr int kLinesPerThread = 1000;

  {
    bilog::FileSink sink(path);

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&sink, t] {
        buf_t buf(bilog::FileSink::kBuffCap);
        for (int i = 0; i < kLinesPerThread; ++i) {
          auto line = "T" + std::to_string(t) + ":" + std::to_string(i) + "\n";
          sink.write(&buf, reinterpret_cast<const std::byte*>(line.data()), line.size());
        }
        sink.flush(&buf);
      });
    }

    for (auto& th : threads) {
      th.join();
    }
  }

  auto content = read_file(path);
  auto line_count = std::ranges::count(content, '\n');
  EXPECT_EQ(line_count, kThreads * kLinesPerThread);

  for (int t = 0; t < kThreads; ++t) {
    for (int i = 0; i < kLinesPerThread; ++i) {
      auto expected = "T" + std::to_string(t) + ":" + std::to_string(i) + "\n";
      EXPECT_NE(content.find(expected), std::string::npos);
    }
  }
  fs::remove(path);
}

TEST(FileSink, NoInterleaving) {
  auto path = temp_filepath("bilog_filesink_interleave.bin");
  constexpr int kThreads = 4;
  constexpr int kWrites = 500;

  {
    bilog::FileSink sink(path);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&sink, t] {
        buf_t buf(bilog::FileSink::kBuffCap);
        char marker = static_cast<char>('A' + t);
        std::string chunk(64, marker);
        chunk += '\n';
        for (int i = 0; i < kWrites; ++i) {
          sink.write(&buf, reinterpret_cast<const std::byte*>(chunk.data()), chunk.size());
        }
        sink.flush(&buf);
      });
    }

    for (auto& th : threads) {
      th.join();
    }
  }

  auto content = read_file(path);

  std::size_t pos = 0;
  int lines = 0;
  while (pos < content.size()) {
    auto nl = content.find('\n', pos);
    if (nl == std::string::npos) break;

    auto line = content.substr(pos, nl - pos);
    ASSERT_EQ(line.size(), 64U) << "Line " << lines << " wrong length";

    char first = line[0];
    EXPECT_TRUE(first >= 'A' && first < 'A' + kThreads) << "Unexpected marker: " << first;
    EXPECT_EQ(line, std::string(64, first)) << "Interleaved data at line " << lines;

    pos = nl + 1;
    ++lines;
  }

  EXPECT_EQ(lines, kThreads * kWrites);
  fs::remove(path);
}
