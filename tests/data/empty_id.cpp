#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("test_output.bin"));

  std::string date = "today";
  bilog::log({})
      .info("Server started")
      .i("port:", 8080U)
      .cs("node:", "stage1")
      .s("date:", date)
      .write();

  bilog::log({}).warn("High memory usage").f("percent:", 85.6F).write();

  bilog::log({}).error("Connection failed").i("retries:", 3U).write();

  return 0;
}
