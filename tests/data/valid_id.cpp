#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("valid_id.bin"));

  std::string date = "today";
  bilog::log({0, 8, 9, 10, 11, 12})
      .info("Server started")
      .i("port:", 8080U)
      .cs("node:", "stage1")
      .s("date:", date)
      .write();

  bilog::log({1, 13, 14})  //
      .warn("High memory usage")
      .f("percent:", 85.6F)
      .write();

  bilog::log({2, 15, 16})  //
      .error("Connection failed")
      .i("retries:", 3U)
      .write();

  return 0;
}
