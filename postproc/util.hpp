#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace postproc {

inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Failed to open " + path.string());
  }
  std::ostringstream buf;
  buf << input.rdbuf();
  return buf.str();
}

}  // namespace postproc
