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
      std::vector<bilog::FieldType> fields;
      for (const auto& item : entry.value.GetArray()) {
        if (!item.IsString()) continue;
        bilog::FieldType ft = bilog::FieldType::Int;
        if (bilog::parse_field_type(item.GetString(), ft)) {
          fields.push_back(ft);
        }
      }
      schema.event_fields[event_id] = std::move(fields);
    }
  }

  return schema;
}

}  // namespace postproc
