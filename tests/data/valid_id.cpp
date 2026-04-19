#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("test_output.bin"));

  std::string date = "today";
  bilog::log({0, 8, 9, 10, 11, 12})
      .info("Server started")
      .num("port:", 8080U)
      .cstr("node:", "stage1")
      .str("date:", date)
      .write();

  bilog::log({1, 13, 14})  //
      .warn("High memory usage")
      .num("percent:", 85.6F)
      .write();

  bilog::log({2, 15, 16})  //
      .error("Connection failed")
      .num("retries:", 3U)
      .write();

  return 0;
}
