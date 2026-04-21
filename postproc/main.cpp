#include <tclap/CmdLine.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "bilog/codec/binary.hpp"
#include "bilog/sink/file.hpp"
#include "schema.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    // clang-format off
    TCLAP::CmdLine cmd(
        "Format bilog binary records into a readable log file", ' ', BILOG_VERSION);
    TCLAP::ValueArg<std::string> schema_arg(
        "s", "schema", "Input log schema JSON file", true, "", "path", cmd);
    TCLAP::ValueArg<std::string> input_arg(
        "i", "input", "Input binary log file", true, "", "path", cmd);
    TCLAP::ValueArg<std::string> output_arg(
        "o", "output", "Output readable log file", false, "", "path", cmd);
    // clang-format on

    cmd.parse(argc, argv);

    const fs::path schema_path = fs::absolute(schema_arg.getValue());
    const fs::path input_path = fs::absolute(input_arg.getValue());

    const auto schema = postproc::load_schema(schema_path);

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
      throw std::runtime_error("Failed to open " + input_path.string());
    }

    bilog::BinaryFormatter formatter(&input);
    bilog::Buffer<bilog::FileSink> buf(bilog::FileSink::kBuffCap);

    if (output_arg.getValue().empty()) {
      std::ofstream stdout_stream;
      stdout_stream.basic_ios<char>::rdbuf(std::cout.rdbuf());
      bilog::FileSink stdout_sink(std::move(stdout_stream));
      std::size_t count = 0;
      while (formatter.format(&buf, &stdout_sink, schema.tag_names, schema.event_fields)) {
        ++count;
      }
      stdout_sink.flush(&buf);
      std::cerr << "Formatted " << count << " log entries to stdout.\n";
    } else {
      const fs::path output_path = fs::absolute(output_arg.getValue());
      bilog::FileSink file_sink(output_path);
      std::size_t count = 0;
      while (formatter.format(&buf, &file_sink, schema.tag_names, schema.event_fields)) {
        ++count;
      }
      file_sink.flush(&buf);

      std::cout << "Formatted " << count << " log entries.\n";
      std::cout << "Output: " << output_path << '\n';
    }
    return 0;
  } catch (const TCLAP::ArgException& ex) {
    std::cerr << ex.error() << " for argument " << ex.argId() << '\n';
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
