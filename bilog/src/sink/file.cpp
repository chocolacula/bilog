#include "bilog/sink/file.hpp"

namespace bilog {

void FileSink::write_file(Buffer<FileSink>* lb, const std::byte* data, std::size_t size) {
  if (size > kBuffCap) {
    flush(lb);

    std::lock_guard<std::mutex> lock(file_mutex_);
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return;
  }
  flush(lb);
  lb->append(data, size);
}

void FileSink::flush(Buffer<FileSink>* lb) {
  if (lb->size() == 0) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_.write(reinterpret_cast<const char*>(lb->data()),
                static_cast<std::streamsize>(lb->size()));
  }
  lb->clear();
}
}  // namespace bilog
