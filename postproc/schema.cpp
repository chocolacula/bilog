#include "schema.hpp"

#include <rapidjson/document.h>

#include "util.hpp"

namespace postproc {

Schema load_schema(const std::filesystem::path& path) {
  Schema schema;
  auto json = read_file(path);

  rapidjson::Document doc;
  doc.Parse(json.data(), json.size());
  if (doc.HasParseError() || !doc.IsObject()) {
    throw std::runtime_error("Failed to parse schema: " + path.string());
  }

  if (doc.HasMember("tags") && doc["tags"].IsObject()) {
    for (const auto& entry : doc["tags"].GetObject()) {
      if (!entry.value.IsString()) continue;
      auto id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      schema.tag_names[id] = entry.value.GetString();
    }
  }

  if (doc.HasMember("events") && doc["events"].IsObject()) {
    for (const auto& entry : doc["events"].GetObject()) {
      if (!entry.value.IsArray()) continue;
      auto event_id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      std::vector<std::size_t> positions;
      for (const auto& pos : entry.value.GetArray()) {
        if (pos.IsUint64()) {
          positions.push_back(static_cast<std::size_t>(pos.GetUint64()));
        }
      }
      schema.event_positions[event_id] = std::move(positions);
    }
  }

  return schema;
}

}  // namespace postproc
