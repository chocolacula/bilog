#include "parser.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

preproc::FileAnalysis analyze(std::string_view content) {
  auto path = bilog::test::temp_path("parser.cpp");
  fs::create_directories(path.parent_path());

  bilog::test::write_file(path, content);
  auto result = preproc::analyze_file(path);

  fs::remove(path);
  return result;
}

}  // namespace

TEST(PreprocParser, EmptyBraces) {
  auto a = analyze("bilog::log({}).info(\"hi\").write();");

  ASSERT_EQ(a.calls.size(), 1U);
  EXPECT_TRUE(a.calls[0].has_write);
  EXPECT_TRUE(a.calls[0].existing_ids.empty());
  ASSERT_EQ(a.calls[0].chain.size(), 1U);
  EXPECT_EQ(a.calls[0].chain[0].method, "info");
  EXPECT_TRUE(a.errors.empty());
}

TEST(PreprocParser, MultipleCalls) {
  auto a = analyze(
      "void f() {\n"
      "  bilog::log({}).info(\"a\").write();\n"
      "  bilog::log({}).warn(\"b\").f(\"p:\", 1.0F).write();\n"
      "}\n");
  ASSERT_EQ(a.calls.size(), 2U);
  EXPECT_EQ(a.calls[0].chain[0].method, "info");
  EXPECT_EQ(a.calls[1].chain[0].method, "warn");
  EXPECT_EQ(a.calls[1].line, 3U);
}

TEST(PreprocParser, ExistingIds) {
  // Exercises parse_id_list's loop, comma handling, and uint literal parsing.
  auto a = analyze("bilog::log({0, 8, 9, 10}).info(\"x\").i(\"n:\", 1U).write();");
  ASSERT_EQ(a.calls.size(), 1U);

  const auto& ids = a.calls[0].existing_ids;
  ASSERT_EQ(ids.size(), 4U);
  EXPECT_EQ(ids[0], 0U);
  EXPECT_EQ(ids[1], 8U);
  EXPECT_EQ(ids[2], 9U);
  EXPECT_EQ(ids[3], 10U);
}

TEST(PreprocParser, ConstString) {
  // produces two chain calls
  auto a = analyze("bilog::log({}).cs(\"node:\", \"stage1\").write();");
  ASSERT_EQ(a.calls.size(), 1U);

  const auto& chain = a.calls[0].chain;
  ASSERT_EQ(chain.size(), 2U);
  EXPECT_EQ(chain[0].method, "cs");
  EXPECT_EQ(chain[0].tag, "node:");
  EXPECT_EQ(chain[1].method, "cs_val");
  EXPECT_EQ(chain[1].tag, "stage1");
}

TEST(PreprocParser, SkipsComments) {
  auto a = analyze(
      "bilog::log({})\n"
      "    // leading comment\n"
      "    .info(\"hi\")\n"
      "    // trailing comment\n"
      "    .write();");
  ASSERT_EQ(a.calls.size(), 1U);
  EXPECT_TRUE(a.calls[0].has_write);
  ASSERT_EQ(a.calls[0].chain.size(), 1U);
  EXPECT_EQ(a.calls[0].chain[0].method, "info");
}

TEST(PreprocParser, MissingWrite) {
  auto a = analyze(R"(bilog::log({}).info("oops").i("n:", 1U);)");
  ASSERT_EQ(a.calls.size(), 1U);
  EXPECT_FALSE(a.calls[0].has_write);
  ASSERT_EQ(a.errors.size(), 1U);
  EXPECT_NE(a.errors[0].find("missing .write()"), std::string::npos);
}

TEST(PreprocParser, RejectsMalformed) {
  auto check = [](const char* name, std::string_view src) {
    SCOPED_TRACE(name);
    auto a = analyze(src);
    EXPECT_TRUE(a.calls.empty());
  };
  check("no paren", "bilog::log");
  check("ident after, no paren", "bilog::log foo");
  check("unmatched open paren", "bilog::log(");
  check("no brace inside parens", "bilog::log(foo)");
  check("unmatched open brace at end", "bilog::log({");
  check("unmatched open brace mid-parens", "bilog::log({)");
  check("empty chain (only .write())", "bilog::log({}).write()");
}

TEST(PreprocParser, UnderscoreMethod) {
  // Method-name scanner accepts isalnum() || '_'. Exercises the `_` branch in
  // both parse_chain_calls and parse_log_call's .write() lookahead scan.
  auto a = analyze("bilog::log({}).my_method(\"tag:\").write();");
  ASSERT_EQ(a.calls.size(), 1U);
  EXPECT_TRUE(a.calls[0].has_write);
  ASSERT_EQ(a.calls[0].chain.size(), 1U);
  EXPECT_EQ(a.calls[0].chain[0].method, "my_method");
  EXPECT_EQ(a.calls[0].chain[0].tag, "tag:");
}
