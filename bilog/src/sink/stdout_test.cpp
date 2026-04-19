#include "bilog/sink/stdout.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <string>

namespace {

using buf_t = bilog::Buffer<bilog::StdoutSink>;

class StdoutCapture {
  int saved_fd_ = -1;
  int pipe_fd_[2] = {-1, -1};

 public:
  void start() {
    (void)std::fflush(stdout);
    saved_fd_ = ::dup(STDOUT_FILENO);
    ::pipe(pipe_fd_);
    ::dup2(pipe_fd_[1], STDOUT_FILENO);
  }

  std::string drain() {
    (void)std::fflush(stdout);
    ::close(pipe_fd_[1]);
    ::dup2(saved_fd_, STDOUT_FILENO);
    ::close(saved_fd_);

    std::string result;
    char buf[256];
    ssize_t n = 0;
    while ((n = ::read(pipe_fd_[0], buf, sizeof(buf))) > 0) {
      result.append(&buf[0], static_cast<std::size_t>(n));
    }
    ::close(pipe_fd_[0]);
    return result;
  }
};

}  // namespace

TEST(StdoutSink, Writes) {
  bilog::StdoutSink sink;
  buf_t buf(bilog::StdoutSink::kBuffCap);
  {
    StdoutCapture capture;
    capture.start();

    sink.write_byte(&buf, static_cast<std::byte>('A'));
    sink.write_byte(&buf, static_cast<std::byte>('B'));
    sink.write_byte(&buf, static_cast<std::byte>('\n'));

    auto output = capture.drain();
    EXPECT_EQ(output, "AB\n");
  }
  {
    StdoutCapture capture;
    capture.start();

    std::string_view msg = "hello world\n";
    sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());

    auto output = capture.drain();
    EXPECT_EQ(output, "hello world\n");
  }
  {
    StdoutCapture capture;
    capture.start();

    std::string_view part1 = "[INFO] hel";
    sink.write(&buf, reinterpret_cast<const std::byte*>(part1.data()), part1.size());
    std::string_view part2 = "lo world\n";
    sink.write(&buf, reinterpret_cast<const std::byte*>(part2.data()), part2.size());

    auto output = capture.drain();
    EXPECT_EQ(output, "[INFO] hello world\n");
  }
}

TEST(StdoutSink, FlushesOnNewline) {
  bilog::StdoutSink sink;
  buf_t buf(bilog::StdoutSink::kBuffCap);

  StdoutCapture capture;
  capture.start();

  std::string_view msg = "no newline";
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  // Not flushed yet — no '\n'
  sink.flush(&buf);  // explicit flush

  auto output = capture.drain();
  EXPECT_EQ(output, "no newline");
}
