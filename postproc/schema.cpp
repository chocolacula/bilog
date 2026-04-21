#include "schema.hpp"

#include <rapidjson/document.h>

#include <stdexcept>
#include <string_view>

#include "util.hpp"

namespace postproc {

inline bilog::FieldType from_str(std::string_view s) {
  if (s == "i") {
    return bilog::FieldType::Int;
  }
  if (s == "f") {
    return bilog::FieldType::Float;
  }
  if (s == "b") {
    return bilog::FieldType::Bool;
  }
  if (s == "s") {
    return bilog::FieldType::String;
  }
  if (s == "cs") {
    return bilog::FieldType::CStr;
  }
  std::string msg = "wrong field type: ";
  msg += s;
  throw std::runtime_error(msg);
}

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
      if (!entry.value.IsString()) {
        continue;
      }
      auto id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      schema.tag_names[id] = entry.value.GetString();
    }
  }

  if (doc.HasMember("events") && doc["events"].IsObject()) {
    for (const auto& entry : doc["events"].GetObject()) {
      if (!entry.value.IsArray()) {
        continue;
      }
      auto event_id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      std::vector<bilog::FieldType> fields;
      for (const auto& item : entry.value.GetArray()) {
        if (!item.IsString()) {
          continue;
        }
        bilog::FieldType ft = bilog::FieldType::Int;
        if (from_str(item.GetString()) == ft) {
          fields.push_back(ft);
        }
      }
      schema.event_fields[event_id] = std::move(fields);
    }
  }
  return schema;
}

}  // namespace postproc
