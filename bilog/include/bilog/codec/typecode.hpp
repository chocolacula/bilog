#pragma once

#include <concepts>
#include <cstdint>
#include <string_view>

#include "bilog/tag.hpp"

namespace bilog {

static constexpr std::uint8_t tc_uint8 = 0x0;
static constexpr std::uint8_t tc_uint16 = 0x1;
static constexpr std::uint8_t tc_uint24 = 0x2;
static constexpr std::uint8_t tc_uint32 = 0x3;
static constexpr std::uint8_t tc_uint40 = 0x4;
static constexpr std::uint8_t tc_uint48 = 0x5;
static constexpr std::uint8_t tc_uint56 = 0x6;
static constexpr std::uint8_t tc_uint64 = 0x7;
static constexpr std::uint8_t tc_float = 0x8;
static constexpr std::uint8_t tc_double = 0x9;
static constexpr std::uint8_t tc_string8 = 0xC;
static constexpr std::uint8_t tc_string16 = 0xD;

template <std::integral T>
static std::uint8_t type_code(const T& val) {
  auto v = static_cast<std::uint64_t>(val);
  if (v < 0xFF) {
    return tc_uint8;
  }
  if (v < 0xFFFF) {
    return tc_uint16;
  }
  if (v < 0xFFFF'FF) {
    return tc_uint24;
  }
  if (v < 0xFFFF'FFFF) {
    return tc_uint32;
  }
  if (v < 0xFFFF'FFFF'FF) {
    return tc_uint40;
  }
  if (v < 0xFFFF'FFFF'FFFF) {
    return tc_uint48;
  }
  if (v < 0xFFFF'FFFF'FFFF'FF) {
    return tc_uint56;
  }
  return tc_uint64;
}

template <std::floating_point T>
static std::uint8_t type_code(const T& /*val*/) {
  if constexpr (std::same_as<T, float>) {
    return tc_float;
  } else {
    return tc_double;
  }
}

template <typename T>
  requires std::convertible_to<T, std::string_view>
static std::uint8_t type_code(const T& val) {
  std::string_view sv(val);
  return sv.size() <= 0xFF ? tc_string8 : tc_string16;
}

static std::uint8_t type_code(std::byte /*val*/) {
  return tc_uint8;
}

static std::uint8_t type_code(const Tag& tag) {
  return type_code(tag.id());
}

}  // namespace bilog
