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
      if (!entry.value.IsString()) continue;
      auto id = static_cast<std::uint64_t>(std::stoull(entry.name.GetString()));
      std::string name = entry.value.GetString();
      schema.tag_ids[name] = id;
      schema.tag_names[id] = name;
    }
  }

  // load events: { "id": [pos, pos, ...], ... }
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
  for (const auto& [event_id, positions] : schema.event_positions) {
    rapidjson::Value pos_arr(rapidjson::kArrayType);
    for (auto pos : positions) {
      pos_arr.PushBack(static_cast<std::uint64_t>(pos), alloc);
    }
    auto id_str = std::to_string(event_id);
    rapidjson::Value key;
    key.SetString(id_str.c_str(), static_cast<rapidjson::SizeType>(id_str.size()), alloc);
    events_obj.AddMember(key, pos_arr, alloc);
  }

  doc.AddMember("tags", tags_obj, alloc);
  doc.AddMember("events", events_obj, alloc);

  rapidjson::StringBuffer buf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
  doc.Accept(writer);
  return buf.GetString();
}

}  // namespace preproc
