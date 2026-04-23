#include "schema.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

#include "bilog/codec/binary.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

TEST(PostprocSchema, TagNames) {
  auto path = bilog::test::temp_path("tags.json");
  bilog::test::write_file(path, R"({
    "tags": {"10": "port:", "20": "host:"},
    "events": {}
  })");

  auto schema = postproc::load_schema(path);
  EXPECT_EQ(schema.tag_names[10], "port:");
  EXPECT_EQ(schema.tag_names[20], "host:");

  fs::remove(path);
}

TEST(PostprocSchema, FieldTypes) {
  auto path = bilog::test::temp_path("all_types.json");
  bilog::test::write_file(path, R"({
    "tags": {"0": "a", "1": "b", "2": "c", "3": "d", "4": "e"},
    "events": {"0": ["i", "f", "b", "s", "cs"]}
  })");

  auto schema = postproc::load_schema(path);
  auto& fields = schema.event_fields[0];

  ASSERT_EQ(fields.size(), 5U);
  EXPECT_EQ(fields[0], bilog::FieldType::Int);
  EXPECT_EQ(fields[1], bilog::FieldType::Float);
  EXPECT_EQ(fields[2], bilog::FieldType::Bool);
  EXPECT_EQ(fields[3], bilog::FieldType::String);
  EXPECT_EQ(fields[4], bilog::FieldType::CStr);

  fs::remove(path);
}

TEST(PostprocSchema, FieldTypeUnknown) {
  auto path = bilog::test::temp_path("bad_type.json");
  bilog::test::write_file(path, R"({
    "tags": {},
    "events": {"0": ["xyz"]}
  })");

  EXPECT_THROW(postproc::load_schema(path), std::runtime_error);

  fs::remove(path);
}

TEST(PostprocSchema, InvalidSchema) {
  auto check = [](const char* name, std::string_view content) {
    SCOPED_TRACE(name);
    auto path = bilog::test::temp_path("invalid.json");
    bilog::test::write_file(path, content);

    EXPECT_THROW(postproc::load_schema(path), std::runtime_error);

    fs::remove(path);
  };
  check("empty", "");
  check("not json", "this is not json");
  check("array", "[1, 2, 3]");
}

TEST(PostprocSchema, EmptySchema) {
  auto check = [](const char* name, std::string_view content) {
    SCOPED_TRACE(name);
    auto path = bilog::test::temp_path("empty.json");
    bilog::test::write_file(path, content);

    auto schema = postproc::load_schema(path);
    EXPECT_TRUE(schema.tag_names.empty());
    EXPECT_TRUE(schema.event_fields.empty());

    fs::remove(path);
  };
  check("empty object", "{}");
  check("missing tags and events", R"({"key":"value"})");
  check("wrong value types", R"({
    "tags": {"0": 42, "1": null},
    "events": {"0": "not an array", "1": 42}
  })");
}

TEST(PostprocSchema, FileNotFoundThrows) {
  EXPECT_THROW(postproc::load_schema("does_not_exist.json"), std::runtime_error);
}
