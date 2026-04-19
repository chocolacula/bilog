#pragma once

#include <cstdint>
#include <string_view>

namespace bilog {

class Tag {
 public:
  Tag(std::uint64_t id, const char* str) : id_(id), str_(str) {}
  Tag(Tag&& tag) noexcept = default;

  [[nodiscard]] std::uint64_t id() const {
    return id_;
  }

  [[nodiscard]] std::string_view str() const {
    return str_;
  }

 private:
  std::uint64_t id_;
  std::string_view str_;
};

}  // namespace bilog
