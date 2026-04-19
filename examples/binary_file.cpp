
#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("bilog_test.bin"));

  bilog::log({0, 0, 1, 2, 3})
      .info("File download succeeded")
      .cstr("filename:", "old_log.log")
      .num("time:", 42U)
      .write();

  bilog::log({1, 4, 5})  //
      .error("shutdown")
      .num("uptime:", 7458596)
      .write();

  bilog::log({2, 6, 7})  //
      .warn("temperature")
      .num("celsius:", 73.5F)
      .write();

  return 0;
}
