#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace preproc {

struct ChainCall {
  std::string method;
  std::string tag;
};

struct LogCall {
  std::size_t brace_open = 0;
  std::size_t brace_close = 0;
  std::vector<std::uint64_t> existing_ids;
  std::vector<ChainCall> chain;
  std::size_t line = 0;
  bool has_write = false;
};

struct FileAnalysis {
  std::filesystem::path path;
  std::string content;
  std::vector<LogCall> calls;
  std::vector<std::string> errors;
};

FileAnalysis analyze_file(const std::filesystem::path& path);

}  // namespace preproc
