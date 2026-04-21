#include "bilog/sink/stdout.hpp"

#include "bilog/codec/text.hpp"

#define BILOG_ENCODER TextEncoder
#define BILOG_SINK StdoutSink
#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::TextEncoder(), bilog::StdoutSink());

  bilog::log({})  //
      .info("File download succeeded,")
      .cs("filename:", "some_image.jpg")
      .i("size:", 42U)
      .write();

  bilog::log({})  //
      .error("Service shutdown,")
      .b("unexpected:", true)
      .i("uptime:", 7458596)
      .write();

  bilog::log({})  //
      .warn("Temperature changed,")
      .f("celsius:", 43.5F)
      .write();

  return 0;
}
