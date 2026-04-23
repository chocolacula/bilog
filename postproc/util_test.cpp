#include "util.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

#include "test_utils.hpp"

namespace fs = std::filesystem;

TEST(PostprocUtil, ReadFile) {
  auto check = [](const char* name, std::string_view content) {
    SCOPED_TRACE(name);
    auto path = bilog::test::temp_path("file.txt");
    bilog::test::write_file(path, content);

    EXPECT_EQ(postproc::read_file(path), content);

    fs::remove(path);
  };
  check("empty", "");
  check("ascii", "hello world");

  auto bytes = std::vector<char>{'\x00', '\x01', 'A', '\x00', '\xFF', '\n'};
  check("binary with nulls", std::string_view(bytes.data(), bytes.size()));
}

TEST(PostprocUtil, ReadFileMissingThrows) {
  auto path = bilog::test::temp_path("does_not_exist.txt");
  EXPECT_THROW(postproc::read_file(path), std::runtime_error);
}
