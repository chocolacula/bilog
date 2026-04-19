#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

inline std::vector<std::byte> read_binary_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    throw std::runtime_error("Failed to open " + path.string());
  }
  auto size = static_cast<std::size_t>(input.tellg());
  input.seekg(0);
  std::vector<std::byte> data(size);
  input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
  return data;
}

}  // namespace postproc
