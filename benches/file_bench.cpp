#include <benchmark/benchmark.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>

#include "bilog/bilog.hpp"

namespace fs = std::filesystem;

namespace {

fs::path log_filepath(const char* file_name) {
  const auto directory = fs::temp_directory_path() / "bilog";
  fs::create_directories(directory);

  auto path = directory / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);

  return path;
}

void bilog_write_file(benchmark::State& state) {
  const auto log_path = log_filepath("bilog.bin");

  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink(log_path));

  for (auto _ : state) {
    bilog::log({0, 1, 2, 3, 4, 5})
        .info("System status updated,")
        .num("request count 1sec:", 42)
        .num("latency ratio 1sec:", std::numbers::pi_v<double>)
        .cstr("last payload:", "alpha beta gamma delta epsilon")
        .write();
  }
}
BENCHMARK(bilog_write_file);

void spdlog_write_file(benchmark::State& state) {
  const auto log_path = log_filepath("spdlog.log");

  auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
  auto logger = std::make_shared<spdlog::logger>("bench", std::move(sink));

  for (auto _ : state) {
    logger->info(
        "System status updated, request count last 1sec: {} latency ratio 1sec: {} last payload: "
        "{}",
        42,
        std::numbers::pi_v<double>,
        "alpha beta gamma delta epsilon");
  }
}
BENCHMARK(spdlog_write_file);

}  // namespace
