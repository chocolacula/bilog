#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser.hpp"

namespace preproc {

struct Schema {
  // tag string -> tag ID (source of truth for stable tag assignment)
  std::unordered_map<std::string, std::uint64_t> tag_ids;

  // reverse: tag ID -> string (for JSON output)
  std::unordered_map<std::uint64_t, std::string> tag_names;

  // event ID -> ordered list of dynamic field type codes ("i","f","b","s","cs")
  std::unordered_map<std::uint64_t, std::vector<std::string>> event_fields;
};

Schema load_schema(const std::filesystem::path& path);
std::string build_schema(const Schema& schema);

}  // namespace preproc
