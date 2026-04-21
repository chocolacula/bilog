#include "schema.hpp"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace preproc {

Schema load_schema(const std::filesystem::path& path) {
  Schema schema;

  if (!std::filesystem::exists(path)) {
    return schema;
  }

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return schema;
  }

  std::ostringstream buf;
  buf << input.rdbuf();
  auto json = buf.str();

  rapidjson::Document doc;
  doc.Parse(json.data(), json.size());
  if (doc.HasParseError() || !doc.IsObject()) {
    return schema;
  }

  // load tags: { "id": "string", ... }
  if (doc.HasMember("tags") && doc["tags"].IsObject()) {
    for (const auto& entry : doc["tags"].GetObject()) {
      if (!entry.value.IsString()) {
        continue;
      }
      auto id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      std::string name = entry.value.GetString();
      schema.tag_ids[name] = id;
      schema.tag_names[id] = name;
    }
  }

  // load events: { "id": ["i", "f", ...], ... }
  if (doc.HasMember("events") && doc["events"].IsObject()) {
    for (const auto& entry : doc["events"].GetObject()) {
      if (!entry.value.IsArray()) {
        continue;
      }
      auto event_id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      std::vector<std::string> fields;
      for (const auto& item : entry.value.GetArray()) {
        if (item.IsString()) {
          fields.emplace_back(item.GetString());
        }
      }
      schema.event_fields[event_id] = std::move(fields);
    }
  }

  return schema;
}

std::string build_schema(const Schema& schema) {
  rapidjson::Document doc;
  doc.SetObject();
  auto& alloc = doc.GetAllocator();

  // tags
  rapidjson::Value tags_obj(rapidjson::kObjectType);
  for (const auto& [id, name] : schema.tag_names) {
    auto id_str = std::to_string(id);
    rapidjson::Value key;
    key.SetString(id_str.c_str(), static_cast<rapidjson::SizeType>(id_str.size()), alloc);
    rapidjson::Value val;
    val.SetString(name.c_str(), static_cast<rapidjson::SizeType>(name.size()), alloc);
    tags_obj.AddMember(key, val, alloc);
  }

  // events
  rapidjson::Value events_obj(rapidjson::kObjectType);
  for (const auto& [event_id, fields] : schema.event_fields) {
    rapidjson::Value arr(rapidjson::kArrayType);
    for (const auto& ft : fields) {
      rapidjson::Value v;
      v.SetString(ft.c_str(), static_cast<rapidjson::SizeType>(ft.size()), alloc);
      arr.PushBack(v, alloc);
    }
    auto id_str = std::to_string(event_id);
    rapidjson::Value key;
    key.SetString(id_str.c_str(), static_cast<rapidjson::SizeType>(id_str.size()), alloc);
    events_obj.AddMember(key, arr, alloc);
  }

  doc.AddMember("tags", tags_obj, alloc);
  doc.AddMember("events", events_obj, alloc);

  rapidjson::StringBuffer buf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
  doc.Accept(writer);
  return buf.GetString();
}

}  // namespace preproc
