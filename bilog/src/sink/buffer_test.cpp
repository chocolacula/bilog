#include "bilog/sink/buffer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>

namespace {

struct DummySink {
  // Satisfies the `sink_->flush(this)` call in Buffer's destructor. The tests
  // never call set_sink(), so this is never invoked at runtime.
  void flush(bilog::Buffer<DummySink>* /*unused*/) {}
};
using buf_t = bilog::Buffer<DummySink>;

}  // namespace

TEST(Buffer, AppendByte) {
  buf_t buf(3);
  buf.append(std::byte{0x11});
  buf.append(std::byte{0x22});
  buf.append(std::byte{0x33});

  ASSERT_EQ(buf.size(), 3U);
  EXPECT_EQ(buf.data()[0], std::byte{0x11});
  EXPECT_EQ(buf.data()[1], std::byte{0x22});
  EXPECT_EQ(buf.data()[2], std::byte{0x33});
}

TEST(Buffer, FillsExactly) {
  buf_t buf(3);
  auto src = std::array{std::byte{7}, std::byte{8}, std::byte{9}};
  buf.append(src.data(), src.size());

  ASSERT_EQ(buf.size(), 3U);
  EXPECT_EQ(std::memcmp(buf.data(), src.data(), 3), 0);
}

TEST(Buffer, DropsOnOverflow) {
  buf_t buf(3);

  buf.append(std::byte{0xAA});
  buf.append(std::byte{0xBB});
  ASSERT_EQ(buf.size(), 2U);

  auto src = std::array{std::byte{1}, std::byte{2}};
  buf.append(src.data(), src.size());
  ASSERT_EQ(buf.size(), 3U);

  buf.append(std::byte{0xCC});

  EXPECT_EQ(buf.size(), 3U);
  EXPECT_EQ(buf.data()[0], std::byte{0xAA});
  EXPECT_EQ(buf.data()[1], std::byte{0xBB});
  EXPECT_EQ(buf.data()[2], std::byte{0x01});
}

TEST(Buffer, ClearResetsCursor) {
  buf_t buf(2);
  buf.append(std::byte{0xFF});
  buf.append(std::byte{0xEE});
  buf.append(std::byte{0xDD});  // dropped
  ASSERT_EQ(buf.size(), 2U);

  buf.clear();
  EXPECT_EQ(buf.size(), 0U);

  // After clear, new appends land at position 0.
  buf.append(std::byte{0x01});
  EXPECT_EQ(buf.size(), 1U);
  EXPECT_EQ(buf.data()[0], std::byte{0x01});
}
