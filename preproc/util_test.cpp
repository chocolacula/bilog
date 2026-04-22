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
  auto check =
      [](std::string_view src, char open, char close, std::optional<std::size_t> expected) {
        auto end = preproc::find_matching(src, 0, open, close);
        EXPECT_EQ(end, expected);
      };
  check("(abc)", '(', ')', 4);
  check("(a(b)c)", '(', ')', 6);
  check("{1, 2, {3}, 4}", '{', '}', 13);
  check("(\")\") foo", '(', ')', 4);       // paren inside string literal is skipped
  check(R"(("a\"b"))", '(', ')', 7);       // escaped quote does not terminate string
  check("(a(b)", '(', ')', std::nullopt);  // escaped quote does not terminate string
}

TEST(PreprocUtil, ParseString) {
  auto check = [](std::string_view src, const std::optional<std::string>& expected) {
    std::size_t cur = 0;
    EXPECT_EQ(preproc::parse_string_literal(src, cur), expected);
  };
  check(R"("hello")", "hello");
  check(R"(   "x")", "x");  // leading whitespace skipped
  check(R"("")", "");
  check(R"("a\\b")", "a\\b");  // escapes
  check(R"("a\"b")", "a\"b");
  check(R"("a\nb")", "a\nb");
  check(R"("a\tb")", "a\tb");
  check(R"("\q")", "q");
  check("foo", std::nullopt);      // no opening quote
  check(R"("abc)", std::nullopt);  // unterminated
}

TEST(PreprocUtil, ParseUint) {
  auto check = [](std::string_view src, std::optional<std::uint64_t> expected) {
    std::size_t cur = 0;
    EXPECT_EQ(preproc::parse_uint_literal(src, cur), expected);
  };
  check("42", 42U);
  check("   123", 123U);  // leading whitespace skipped
  check("1'000'000", 1'000'000U);
  check("8080UL", 8080U);      // suffix letters consumed as part of the token
  check("xyz", std::nullopt);  // no digits — cur stays at 0
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
