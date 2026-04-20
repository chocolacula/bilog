
#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("bilog_test.bin"));

  bilog::log({0, 0, 1, 2, 3})
      .info("File download succeeded,")
      .cs("filename:", "some_image.jpg")
      .i("size:", 42U)
      .write();

  bilog::log({1, 4, 5, 6})  //
      .error("Service shutdown,")
      .b("unexpected:", true)
      .i("uptime:", 7458596)
      .write();

  bilog::log({2, 7, 8})  //
      .warn("Temperature changed,")
      .f("celsius:", 43.5F)
      .write();

  return 0;
}
