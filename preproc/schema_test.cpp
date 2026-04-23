#include "schema.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

#include "test_utils.hpp"

namespace fs = std::filesystem;

TEST(PreprocSchema, Load) {
  auto path = bilog::test::temp_path("roundtrip.json");
  bilog::test::write_file(path, R"({
    "tags": {"10": "port:", "20": "host:"},
    "events": {"0": ["i", "cs"], "1": ["f"]}
  })");

  auto schema = preproc::load_schema(path);

  EXPECT_EQ(schema.tag_ids["port:"], 10U);
  EXPECT_EQ(schema.tag_ids["host:"], 20U);
  EXPECT_EQ(schema.tag_names[10], "port:");
  EXPECT_EQ(schema.tag_names[20], "host:");

  ASSERT_EQ(schema.event_fields[0].size(), 2U);
  EXPECT_EQ(schema.event_fields[0][0], "i");
  EXPECT_EQ(schema.event_fields[0][1], "cs");
  ASSERT_EQ(schema.event_fields[1].size(), 1U);
  EXPECT_EQ(schema.event_fields[1][0], "f");

  fs::remove(path);
}

TEST(PreprocSchema, MissingFile) {
  auto schema = preproc::load_schema("does_not_exist.json");
  EXPECT_TRUE(schema.tag_ids.empty());
  EXPECT_TRUE(schema.tag_names.empty());
  EXPECT_TRUE(schema.event_fields.empty());
}

TEST(PreprocSchema, Malformed) {
  auto check = [](const char* name, std::string_view content) {
    SCOPED_TRACE(name);
    auto path = bilog::test::temp_path("malformed.json");
    bilog::test::write_file(path, content);

    auto schema = preproc::load_schema(path);
    EXPECT_TRUE(schema.tag_ids.empty());
    EXPECT_TRUE(schema.tag_names.empty());
    EXPECT_TRUE(schema.event_fields.empty());

    fs::remove(path);
  };
  check("empty file", "");
  check("garbage", "this is not json");
  check("array", "[1, 2, 3]");
  check("number", "42");
}

TEST(PreprocSchema, IgnoreWrongTypes) {
  // Each bad entry is silently skipped,
  // good entries in the same object are still loaded.
  auto path = bilog::test::temp_path("mixed.json");
  bilog::test::write_file(path, R"({
    "tags": {"1": "ok:", "2": 42, "3": null, "4": "also_ok:"},
    "events": {
      "0": ["i", 42, "cs", null],
      "1": "not an array",
      "2": ["f"]
    }
  })");

  auto schema = preproc::load_schema(path);

  // Tag 1 and 4 load; 2 and 3 are dropped.
  ASSERT_EQ(schema.tag_names.size(), 2U);
  EXPECT_EQ(schema.tag_names.at(1), "ok:");
  EXPECT_EQ(schema.tag_names.at(4), "also_ok:");

  // Event 0 keeps only string entries; event 1 (not an array) is skipped
  // entirely; event 2 loads.
  ASSERT_EQ(schema.event_fields.size(), 2U);
  EXPECT_FALSE(schema.event_fields.contains(1));
  ASSERT_EQ(schema.event_fields.at(0).size(), 2U);
  EXPECT_EQ(schema.event_fields.at(0)[0], "i");
  EXPECT_EQ(schema.event_fields.at(0)[1], "cs");
  ASSERT_EQ(schema.event_fields.at(2).size(), 1U);
  EXPECT_EQ(schema.event_fields.at(2)[0], "f");

  fs::remove(path);
}

TEST(PreprocSchema, RebuildSchema) {
  // Roundtrip: build -> parse -> same contents.
  preproc::Schema original;
  original.tag_names[1] = "port:";
  original.tag_names[2] = "host:";
  original.tag_ids["port:"] = 1;
  original.tag_ids["host:"] = 2;
  original.event_fields[0] = {"i", "cs"};
  original.event_fields[7] = {"f", "b"};

  auto json = preproc::build_schema(original);

  auto path = bilog::test::temp_path("built.json");
  bilog::test::write_file(path, json);
  auto reloaded = preproc::load_schema(path);

  EXPECT_EQ(reloaded.tag_names, original.tag_names);
  EXPECT_EQ(reloaded.tag_ids, original.tag_ids);
  EXPECT_EQ(reloaded.event_fields, original.event_fields);

  fs::remove(path);
}
