#include "bilog/sink/stdout.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <string_view>

#if defined(__APPLE__)
#include <util.h>  // openpty() on macOS
#else
#include <pty.h>  // openpty() on Linux (libutil / libc)
#endif

namespace {

using buf_t = bilog::Buffer<bilog::StdoutSink>;

class PipeCapture {
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

class TtyCapture {
  int saved_fd_ = -1;
  int master_fd_ = -1;
  int slave_fd_ = -1;

 public:
  static bool supported() {
    int master_fd = -1;
    int slave_fd = -1;

    bool opened = ::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0;
    if (opened) {
      ::close(slave_fd);
      ::close(master_fd);
    }
    return opened;
  }

  void start() {
    (void)std::fflush(stdout);
    ::openpty(&master_fd_, &slave_fd_, nullptr, nullptr, nullptr);

    // Put the master in non-blocking mode up front so a read after flush
    // returns immediately once the kernel buffer is empty rather than hanging.
    int flags = ::fcntl(master_fd_, F_GETFL, 0);
    (void)::fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);

    saved_fd_ = ::dup(STDOUT_FILENO);
    ::dup2(slave_fd_, STDOUT_FILENO);
  }

  std::string drain() {
    (void)std::fflush(stdout);

    // Read everything that's currently in the master buffer, BEFORE closing
    // the slave. On some kernels closing the slave flushes pending data.
    std::string result;
    char buf[512];
    ssize_t n = 0;
    while ((n = ::read(master_fd_, buf, sizeof(buf))) > 0) {
      result.append(&buf[0], static_cast<std::size_t>(n));
    }

    ::dup2(saved_fd_, STDOUT_FILENO);
    ::close(saved_fd_);
    ::close(slave_fd_);
    ::close(master_fd_);
    return result;
  }
};

}  // namespace

// --- non-TTY: plain pass-through, no ANSI codes ---------------------------

TEST(StdoutSink, Writes) {
  bilog::StdoutSink sink;
  buf_t buf(bilog::StdoutSink::kBuffCap);
  {
    PipeCapture capture;
    capture.start();

    sink.write_byte(&buf, static_cast<std::byte>('A'));
    sink.write_byte(&buf, static_cast<std::byte>('B'));
    sink.write_byte(&buf, static_cast<std::byte>('\n'));
    sink.commit(&buf);

    auto output = capture.drain();
    EXPECT_EQ(output, "AB\n");
  }
  {
    PipeCapture capture;
    capture.start();

    std::string_view msg = "hello world\n";
    sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
    sink.commit(&buf);

    auto output = capture.drain();
    EXPECT_EQ(output, "hello world\n");
  }
  {
    PipeCapture capture;
    capture.start();

    std::string_view part1 = "[INFO] hel";
    sink.write(&buf, reinterpret_cast<const std::byte*>(part1.data()), part1.size());
    std::string_view part2 = "lo world\n";
    sink.write(&buf, reinterpret_cast<const std::byte*>(part2.data()), part2.size());
    sink.commit(&buf);

    auto output = capture.drain();
    // Non-TTY path: no color codes injected even though the line starts with a level tag.
    EXPECT_EQ(output, "[INFO] hello world\n");
  }
}

TEST(StdoutSink, FlushEmptyBuffer) {
  bilog::StdoutSink sink;
  buf_t buf(bilog::StdoutSink::kBuffCap);

  PipeCapture capture;
  capture.start();

  sink.flush(&buf);

  EXPECT_EQ(capture.drain(), "");
}

TEST(StdoutSink, Color) {
  if (!TtyCapture::supported()) {
    GTEST_SKIP() << "openpty failed; skipping TTY color test";
  }

  struct Case {
    std::string_view tag;
    std::string_view color;
  };
  const Case cases[] = {
      {.tag = "[TRACE]", .color = "\033[37m"},    //
      {.tag = "[DEBUG]", .color = "\033[36m"},    //
      {.tag = "[INFO]", .color = "\033[32m"},     //
      {.tag = "[WARN]", .color = "\033[33m"},     //
      {.tag = "[ERROR]", .color = "\033[31m"},    //
      {.tag = "[FATAL]", .color = "\033[1;31m"},  //
  };
  constexpr std::string_view kReset = "\033[0m";

  for (const auto& c : cases) {
    TtyCapture capture;
    capture.start();

    bilog::StdoutSink sink;
    buf_t buf(bilog::StdoutSink::kBuffCap);

    std::string line = std::string(c.tag) + " message\n";
    sink.write(&buf, reinterpret_cast<const std::byte*>(line.data()), line.size());
    sink.commit(&buf);

    auto output = capture.drain();
    EXPECT_NE(output.find(c.color), std::string::npos) << "missing color code for " << c.tag;
    EXPECT_NE(output.find(c.tag), std::string::npos) << "missing tag " << c.tag;
    EXPECT_NE(output.find(kReset), std::string::npos) << "missing reset after " << c.tag;
  }
}

TEST(StdoutSink, ColorUnknownPrefix) {
  if (!TtyCapture::supported()) {
    GTEST_SKIP() << "openpty failed; skipping TTY passthrough test";
  }
  TtyCapture capture;
  capture.start();

  bilog::StdoutSink sink;
  buf_t buf(bilog::StdoutSink::kBuffCap);

  // No level tag at the start → color branch exits the loop without matching
  std::string_view msg = "plain line\n";
  sink.write(&buf, reinterpret_cast<const std::byte*>(msg.data()), msg.size());
  sink.commit(&buf);

  auto output = capture.drain();
  EXPECT_NE(output.find("plain line"), std::string::npos);
  EXPECT_EQ(output.find("\033["), std::string::npos);
}
