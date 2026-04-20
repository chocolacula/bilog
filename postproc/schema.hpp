#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "bilog/codec/binary.hpp"

namespace postproc {

struct Schema {
  std::unordered_map<std::uint64_t, std::string> tag_names;
  std::unordered_map<std::uint64_t, std::vector<bilog::FieldType>> event_fields;
};

Schema load_schema(const std::filesystem::path& path);

}  // namespace postproc
