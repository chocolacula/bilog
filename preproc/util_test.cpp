#include "util.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "test_utils.hpp"

namespace fs = std::filesystem;

TEST(PreprocUtil, IsCppSource) {
  EXPECT_TRUE(preproc::is_cpp_source("foo.cpp"));
  EXPECT_TRUE(preproc::is_cpp_source("foo.cc"));
  EXPECT_TRUE(preproc::is_cpp_source("foo.cxx"));
  EXPECT_TRUE(preproc::is_cpp_source("foo.hpp"));
  EXPECT_TRUE(preproc::is_cpp_source("foo.hh"));
  EXPECT_TRUE(preproc::is_cpp_source("foo.hxx"));
  EXPECT_TRUE(preproc::is_cpp_source("/path/to/src/foo.cpp"));

  EXPECT_FALSE(preproc::is_cpp_source("foo.c"));
  EXPECT_FALSE(preproc::is_cpp_source("foo.h"));
  EXPECT_FALSE(preproc::is_cpp_source("foo.txt"));
  EXPECT_FALSE(preproc::is_cpp_source("foo"));
  EXPECT_FALSE(preproc::is_cpp_source(""));
}

TEST(PreprocUtil, ReadFile) {
  auto path = bilog::test::temp_path("rw.cpp");

  preproc::write_file(path, "hello\nworld\n");
  EXPECT_EQ(preproc::read_file(path), "hello\nworld\n");

  fs::remove(path);
}

TEST(PreprocUtil, ReadFileMissing) {
  auto path = bilog::test::temp_path("missing.cpp");
  EXPECT_THROW(preproc::read_file(path), std::runtime_error);
}

TEST(PreprocUtil, WriteFileTruncates) {
  auto path = bilog::test::temp_path("trunc.cpp");

  preproc::write_file(path, "abcdefghij");
  preproc::write_file(path, "xyz");

  EXPECT_EQ(preproc::read_file(path), "xyz");

  fs::remove(path);
}

TEST(PreprocUtil, LineNumberAt) {
  std::string_view src = "line1\nline2\nline3\n";
  EXPECT_EQ(preproc::line_number_at(src, 0), 1U);
  EXPECT_EQ(preproc::line_number_at(src, 5), 1U);   // at '\n' of line 1
  EXPECT_EQ(preproc::line_number_at(src, 6), 2U);   // start of line 2
  EXPECT_EQ(preproc::line_number_at(src, 12), 3U);  // start of line 3
  EXPECT_EQ(preproc::line_number_at(src, src.size()), 4U);
}

TEST(PreprocUtil, SkipWs) {
  std::string_view src = "   \t\n foo   ";
  std::size_t cur = 0;

  preproc::skip_ws(src, cur);
  EXPECT_EQ(cur, 6U);

  // stay the same
  preproc::skip_ws(src, cur);
  EXPECT_EQ(cur, 6U);

  // reach end
  cur += 3;
  preproc::skip_ws(src, cur);
  EXPECT_EQ(cur, src.size());
}

TEST(PreprocUtil, ConsumeLiteral) {
  std::size_t cur = 0;

  // stay the same
  EXPECT_FALSE(preproc::consume_literal("xyz", cur, "bilog"));
  EXPECT_EQ(cur, 0);

  std::string_view src = "bilog::log(x)";
  EXPECT_TRUE(preproc::consume_literal(src, cur, ""));
  EXPECT_EQ(cur, 0);

  EXPECT_TRUE(preproc::consume_literal(src, cur, "bilog::log"));
  EXPECT_EQ(cur, 10U);

  EXPECT_TRUE(preproc::consume_literal(src, cur, "(x)"));
  EXPECT_EQ(cur, 13U);

  EXPECT_TRUE(preproc::consume_literal(src, cur, ""));
  EXPECT_EQ(cur, 13U);
}

TEST(PreprocUtil, FindMatching) {
  auto check = [](const char* name,
                  std::string_view src,
                  char open,
                  char close,
                  std::optional<std::size_t> expected) {
    SCOPED_TRACE(name);
    EXPECT_EQ(preproc::find_matching(src, 0, open, close), expected);
  };
  check("flat parens", "(abc)", '(', ')', 4);
  check("nested parens", "(a(b)c)", '(', ')', 6);
  check("nested braces", "{1, 2, {3}, 4}", '{', '}', 13);
  check("paren inside string literal", "(\")\") foo", '(', ')', 4);
  check("escaped quote inside string", R"(("a\"b"))", '(', ')', 7);
  check("unbalanced", "(a(b)", '(', ')', std::nullopt);
}

TEST(PreprocUtil, ParseString) {
  auto check = [](const char* name,
                  std::string_view src,
                  const std::optional<std::string>& expected) {
    SCOPED_TRACE(name);
    std::size_t cur = 0;
    EXPECT_EQ(preproc::parse_string_literal(src, cur), expected);
  };
  check("plain", R"("hello")", "hello");
  check("leading whitespace", R"(   "x")", "x");
  check("empty", R"("")", "");
  check("backslash escape", R"("a\\b")", "a\\b");
  check("quote escape", R"("a\"b")", "a\"b");
  check("newline escape", R"("a\nb")", "a\nb");
  check("tab escape", R"("a\tb")", "a\tb");
  check("unknown escape falls back to char", R"("\q")", "q");
  check("no opening quote", "foo", std::nullopt);
  check("unterminated", R"("abc)", std::nullopt);
}

TEST(PreprocUtil, ParseUint) {
  auto check = [](const char* name,
                  std::string_view src,
                  std::optional<std::uint64_t> expected) {
    SCOPED_TRACE(name);
    std::size_t cur = 0;
    EXPECT_EQ(preproc::parse_uint_literal(src, cur), expected);
  };
  check("plain", "42", 42U);
  check("leading whitespace", "   123", 123U);
  check("digit separator", "1'000'000", 1'000'000U);
  check("with suffix", "8080UL", 8080U);
  check("no digits", "xyz", std::nullopt);
}

TEST(PreprocUtil, CollectSourcesFile) {
  auto dir = bilog::test::temp_path("collect_single");
  fs::create_directory(dir);

  auto f = dir / "a.cpp";
  bilog::test::write_file(f, "");

  auto files = preproc::collect_sources(f);
  ASSERT_EQ(files.size(), 1U);
  EXPECT_EQ(files[0], fs::absolute(f));

  fs::remove_all(dir);
}

TEST(PreprocUtil, CollectSourcesDirectory) {
  auto dir = bilog::test::temp_path("collect_dir");
  fs::create_directory(dir);

  bilog::test::write_file(dir / "a.cpp", "");
  bilog::test::write_file(dir / "b.hpp", "");
  bilog::test::write_file(dir / "c.txt", "");
  bilog::test::write_file(dir / "d.h", "");

  auto files = preproc::collect_sources(dir);
  std::ranges::sort(files);

  ASSERT_EQ(files.size(), 2U);
  EXPECT_EQ(files[0].filename(), "a.cpp");
  EXPECT_EQ(files[1].filename(), "b.hpp");

  fs::remove_all(dir);
}

TEST(PreprocUtil, CollectSourcesRecursive) {
  // Walks the whole tree; extension filter applies at every depth.
  auto dir = bilog::test::temp_path("collect_recurse");
  fs::create_directories(dir / "sub" / "deep");

  bilog::test::write_file(dir / "root.cpp", "");
  bilog::test::write_file(dir / "sub" / "nested.cpp", "");
  bilog::test::write_file(dir / "sub" / "deep" / "deeper.hpp", "");
  bilog::test::write_file(dir / "sub" / "skip.txt", "");

  auto files = preproc::collect_sources(dir);
  std::ranges::sort(files);

  ASSERT_EQ(files.size(), 3U);
  EXPECT_EQ(files[0].filename(), "root.cpp");
  EXPECT_EQ(files[1].filename(), "deeper.hpp");
  EXPECT_EQ(files[2].filename(), "nested.cpp");

  fs::remove_all(dir);
}
