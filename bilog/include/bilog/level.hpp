#pragma once

#include <compare>
#include <optional>
#include <string_view>

namespace bilog {

class Level {
  std::byte val_;
  constexpr explicit Level(std::byte val) noexcept : val_(val) {}

 public:
  constexpr std::optional<std::string_view> to_str() const {
    static const std::string_view levels[] = {"TRACE",  //
                                              "DEBUG",
                                              "INFO",
                                              "WARN",
                                              "ERROR",
                                              "FATAL"};

    return levels  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        [static_cast<size_t>(val_)];
  }
  constexpr std::byte to_byte() const {
    return val_;
  }

  constexpr friend auto operator<=>(Level lhs, Level rhs) {
    return lhs.val_ <=> rhs.val_;
  }

  constexpr friend bool operator==(Level lhs, Level rhs) = default;

  static std::optional<Level> from_byte(std::byte val) {
    if (val >= kTrace.to_byte() and val <= kFatal.to_byte()) {
      return Level(val);
    }
    return std::nullopt;
  }

  static const Level kTrace;
  static const Level kDebug;
  static const Level kInfo;
  static const Level kWarn;
  static const Level kError;
  static const Level kFatal;
};

inline constexpr Level Level::kTrace(static_cast<std::byte>(0));
inline constexpr Level Level::kDebug(static_cast<std::byte>(1));
inline constexpr Level Level::kInfo(static_cast<std::byte>(2));
inline constexpr Level Level::kWarn(static_cast<std::byte>(3));
inline constexpr Level Level::kError(static_cast<std::byte>(4));
inline constexpr Level Level::kFatal(static_cast<std::byte>(5));

}  // namespace bilog
