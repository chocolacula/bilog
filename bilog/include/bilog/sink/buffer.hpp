#pragma once

#include <cstddef>
#include <cstring>
#include <new>

namespace bilog {

template <typename SinkT>
class Buffer {
  std::byte* data_ = nullptr;
  size_t cursor_{};
  size_t cap_{};
  SinkT* sink_ = nullptr;

 public:
  explicit Buffer(std::size_t size)
      : data_(new (std::align_val_t(64)) std::byte[size]), cap_(size) {}
  Buffer(Buffer&& other) = default;
  Buffer& operator=(Buffer&& other) = default;

  ~Buffer() {
    if (sink_ != nullptr) {
      sink_->flush(this);
    }
    delete[] data_;
  }

  void set_sink(SinkT* sink) {
    sink_ = sink;
  }

  void append(std::byte b) {
    if (cursor_ >= cap_) [[unlikely]] {
      return;
    }
    append_unsafe(b);
  }

  void append_unsafe(std::byte b) {
    data_[cursor_++] = b;
  }

  void append(const std::byte* data, size_t size) {
    if (cursor_ + size > cap_) [[unlikely]] {
      append_unsafe(data, cap_ - cursor_);
      return;
    }
    append_unsafe(data, size);
  }

  void append_unsafe(const std::byte* data, size_t size) {
    std::memcpy(data_ + cursor_, data, size);
    cursor_ += size;
  }

  [[nodiscard]] const std::byte* data() const {
    return data_;
  }

  [[nodiscard]] std::size_t size() const {
    return cursor_;
  }

  void clear() {
    cursor_ = 0;
  }
};

}  // namespace bilog
