#include <tclap/CmdLine.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include "parser.hpp"
#include "rewriter.hpp"
#include "schema.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    // clang-format off
    TCLAP::CmdLine cmd(
        "Scan C++ sources for bilog::log() calls and generate schema", ' ', BILOG_VERSION);
    TCLAP::ValueArg<std::string> input_arg(
        "i", "input", "Input source file or directory", false, ".", "path", cmd);
    TCLAP::ValueArg<std::string> output_arg(
        "o", "output", "Output schema JSON file", false, "bilog.json", "path", cmd);
    TCLAP::ValueArg<unsigned int> threads_arg(
        "j", "jobs", "Threads count", false, std::thread::hardware_concurrency(), "count", cmd);
    // clang-format on

    cmd.parse(argc, argv);

    const fs::path source_root = fs::absolute(input_arg.getValue());
    const fs::path output_json = fs::absolute(output_arg.getValue());
    const auto requested_threads = threads_arg.getValue();
    const std::size_t thread_count =
        std::max<std::size_t>(1, static_cast<std::size_t>(requested_threads));

    const auto files = preproc::collect_sources(source_root);
    if (files.empty()) {
      std::cerr << "No C++ source files found under " << source_root << '\n';
      return 1;
    }

    // parallel file analysis
    std::vector<preproc::FileAnalysis> analyses(files.size());
    std::atomic<std::size_t> next_index{0};
    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for (std::size_t w = 0; w < thread_count; ++w) {
      workers.emplace_back([&] {
        while (true) {
          const std::size_t i = next_index.fetch_add(1);
          if (i >= files.size()) {
            break;
          }
          analyses[i] = preproc::analyze_file(files[i]);
        }
      });
    }
    for (auto& w : workers) {
      w.join();
    }

    // check for errors
    bool has_errors = false;
    for (const auto& a : analyses) {
      for (const auto& err : a.errors) {
        std::cerr << "error: " << err << '\n';
        has_errors = true;
      }
    }
    if (has_errors) {
      return 1;
    }

    // load existing schema for stable ID assignment
    auto schema = preproc::load_schema(output_json);

    // assign IDs and rewrite sources
    preproc::assign_ids(schema, analyses);
    preproc::rewrite_sources(analyses);

    // save updated schema
    preproc::write_file(output_json, preproc::build_schema(schema));

    // summary
    std::size_t call_count = 0;
    for (const auto& a : analyses) {
      call_count += a.calls.size();
    }

    std::cout << "Scanned " << files.size() << " files with " << thread_count << " threads.\n";
    std::cout << "Found " << call_count << " bilog::log() calls.\n";
    std::cout << "Schema: " << output_json << '\n';
    return 0;
  } catch (const TCLAP::ArgException& ex) {
    std::cerr << ex.error() << " for argument " << ex.argId() << '\n';
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
