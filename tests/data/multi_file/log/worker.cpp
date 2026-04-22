#include "bilog/bilog.hpp"
#include "helpers.hpp"

void log_worker_events() {
  bilog::log({}).warn("High memory usage").cs("node:", "stage1").f("percent:", 85.6F).write();

  bilog::log({}).error("Connection failed").i("retries:", 3U).write();
}
