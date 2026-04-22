#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace preproc {

inline bool is_cpp_source(const fs::path& path) {
  const auto ext = path.extension().string();
  return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".hh" ||
         ext == ".hxx";
}

inline std::string read_file(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Failed to open " + path.string());
  }
  std::ostringstream buf;
  buf << input.rdbuf();
  return buf.str();
}

inline void write_file(const fs::path& path, const std::string& content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("Failed to write " + path.string());
  }
  output << content;
}

inline std::size_t line_number_at(std::string_view content, std::size_t offset) {
  return 1 + static_cast<std::size_t>(std::count(
                 content.begin(), content.begin() + static_cast<std::ptrdiff_t>(offset), '\n'));
}

inline void skip_ws(std::string_view src, std::size_t& cur) {
  while (cur < src.size() && std::isspace(static_cast<unsigned char>(src[cur])) != 0) {
    ++cur;
  }
}

inline bool consume_literal(std::string_view src, std::size_t& cur, std::string_view lit) {
  if (src.substr(cur, lit.size()) != lit) {
    return false;
  }
  cur += lit.size();
  return true;
}

inline std::optional<std::size_t> find_matching(std::string_view src,
                                                std::size_t open,
                                                char open_ch,
                                                char close_ch) {
  int depth = 0;
  bool in_string = false;
  bool escaping = false;

  for (std::size_t i = open; i < src.size(); ++i) {
    const char ch = src[i];
    if (in_string) {
      if (escaping) {
        escaping = false;
        continue;
      }
      if (ch == '\\') {
        escaping = true;
        continue;
      }
      if (ch == '"') {
        in_string = false;
      }
      continue;
    }
    if (ch == '"') {
      in_string = true;
      continue;
    }
    if (ch == open_ch) {
      ++depth;
    } else if (ch == close_ch) {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return std::nullopt;
}

inline std::optional<std::string> parse_string_literal(std::string_view src, std::size_t& cur) {
  skip_ws(src, cur);
  if (cur >= src.size() || src[cur] != '"') {
    return std::nullopt;
  }
  ++cur;
  std::string result;
  bool escaping = false;
  while (cur < src.size()) {
    const char ch = src[cur++];
    if (escaping) {
      switch (ch) {
        case '\\':
        case '"':
          result.push_back(ch);
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          result.push_back(ch);
          break;
      }
      escaping = false;
      continue;
    }
    if (ch == '\\') {
      escaping = true;
      continue;
    }
    if (ch == '"') {
      return result;
    }
    result.push_back(ch);
  }
  return std::nullopt;
}

inline std::optional<std::uint64_t> parse_uint_literal(std::string_view src, std::size_t& cur) {
  skip_ws(src, cur);
  const std::size_t start = cur;
  while (cur < src.size()) {
    const char ch = src[cur];
    if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '\'') {
      ++cur;
    } else {
      break;
    }
  }
  if (cur == start) {
    return std::nullopt;
  }
  while (cur < src.size() && std::isalpha(static_cast<unsigned char>(src[cur])) != 0) {
    ++cur;
  }
  std::string digits;
  for (std::size_t i = start; i < cur; ++i) {
    if (src[i] != '\'') {
      digits.push_back(src[i]);
    }
  }
  return static_cast<std::uint64_t>(std::stoull(digits));
}

inline std::vector<fs::path> collect_sources(const fs::path& root) {
  std::vector<fs::path> files;

  if (fs::is_regular_file(root) && is_cpp_source(root)) {
    files.push_back(fs::absolute(root));
    return files;
  }

  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && is_cpp_source(entry.path())) {
      files.push_back(fs::absolute(entry.path()));
    }
  }
  return files;
}

}  // namespace preproc
