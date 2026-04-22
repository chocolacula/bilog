#include "bilog/bilog.hpp"
#include "log/helpers.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("log.bin"));

  log_server_events();
  log_worker_events();

  return 0;
}
