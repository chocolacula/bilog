#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace postproc {

struct Schema {
  std::unordered_map<std::uint64_t, std::string> tag_names;
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> event_positions;
};

Schema load_schema(const std::filesystem::path& path);

}  // namespace postproc
