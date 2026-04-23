#include "bilog/level.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

TEST(Level, FromByte) {
  auto check = [](const char* name, std::uint8_t byte, std::optional<bilog::Level> expected) {
    SCOPED_TRACE(name);
    EXPECT_EQ(bilog::Level::from_byte(static_cast<std::byte>(byte)), expected);
  };
  check("trace", 0, bilog::Level::kTrace);
  check("debug", 1, bilog::Level::kDebug);
  check("info", 2, bilog::Level::kInfo);
  check("warn", 3, bilog::Level::kWarn);
  check("error", 4, bilog::Level::kError);
  check("fatal", 5, bilog::Level::kFatal);
  check("one past last valid", 6, std::nullopt);
  check("top of byte range", 255, std::nullopt);
}

TEST(Level, FromByteRoundtrip) {
  auto check = [](const char* name, bilog::Level level) {
    SCOPED_TRACE(name);
    auto round = bilog::Level::from_byte(level.to_byte());
    ASSERT_TRUE(round.has_value());
    EXPECT_EQ(*round, level);
    EXPECT_EQ(round->to_str(), level.to_str());
  };
  check("trace", bilog::Level::kTrace);
  check("debug", bilog::Level::kDebug);
  check("info", bilog::Level::kInfo);
  check("warn", bilog::Level::kWarn);
  check("error", bilog::Level::kError);
  check("fatal", bilog::Level::kFatal);
}

TEST(Level, ToStr) {
  auto check = [](const char* name, bilog::Level level, std::string_view expected) {
    SCOPED_TRACE(name);
    EXPECT_EQ(level.to_str(), expected);
  };
  check("trace", bilog::Level::kTrace, "TRACE");
  check("debug", bilog::Level::kDebug, "DEBUG");
  check("info", bilog::Level::kInfo, "INFO");
  check("warn", bilog::Level::kWarn, "WARN");
  check("error", bilog::Level::kError, "ERROR");
  check("fatal", bilog::Level::kFatal, "FATAL");
}

TEST(Level, Comparison) {
  EXPECT_EQ(bilog::Level::kInfo, bilog::Level::kInfo);
  EXPECT_NE(bilog::Level::kInfo, bilog::Level::kWarn);
  EXPECT_LT(bilog::Level::kTrace, bilog::Level::kFatal);
  EXPECT_GT(bilog::Level::kError, bilog::Level::kDebug);
  EXPECT_LE(bilog::Level::kInfo, bilog::Level::kInfo);
  EXPECT_GE(bilog::Level::kInfo, bilog::Level::kInfo);
}
