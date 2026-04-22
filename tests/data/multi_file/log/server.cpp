#include "bilog/bilog.hpp"
#include "helpers.hpp"

void log_server_events() {
  bilog::log({}).info("Server started").i("port:", 8080U).cs("node:", "stage1").write();

  bilog::log({}).warn("Config reloaded").cs("node:", "stage1").write();
}
