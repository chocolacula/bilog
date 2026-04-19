#include "bilog/sink/stdout.hpp"

#include <unistd.h>

namespace bilog {

StdoutSink::StdoutSink() : color_(::isatty(STDOUT_FILENO) != 0) {}

void StdoutSink::flush(Buffer<StdoutSink>* lb) {
  if (lb->size() == 0) {
    return;
  }

  auto sv = std::string_view(reinterpret_cast<const char*>(lb->data()), lb->size());

  if (color_) {
    for (std::size_t i = 0; i < std::size(kTags); ++i) {
      if (sv.starts_with(kTags[i])) {
        (void)std::fwrite(kColors[i].data(), 1, kColors[i].size(), stdout);
        (void)std::fwrite(kTags[i].data(), 1, kTags[i].size(), stdout);
        (void)std::fwrite(kReset.data(), 1, kReset.size(), stdout);

        auto rest = sv.substr(kTags[i].size());
        (void)std::fwrite(rest.data(), 1, rest.size(), stdout);
        lb->clear();
        return;
      }
    }
  }

  (void)std::fwrite(lb->data(), 1, lb->size(), stdout);
  lb->clear();
}

}  // namespace bilog
