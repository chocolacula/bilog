#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "bilog/sink/buffer.hpp"
#include "bilog/sink/ringbuff.hpp"

namespace bilog::test {

/// Flush staging buffer and drain ring buffer into a contiguous byte vector.
inline std::vector<std::byte> drain(Buffer<RingBuffSink>* buf, RingBuffSink* sink) {
  sink->flush(buf);
  std::vector<std::byte> out(sink->available());
  sink->read(out.data(), out.size());
  return out;
}

/// Flush staging buffer and drain ring buffer into a string.
inline std::string drain_str(Buffer<RingBuffSink>* buf, RingBuffSink* sink) {
  sink->flush(buf);
  std::string result(sink->available(), '\0');
  sink->read(reinterpret_cast<std::byte*>(result.data()), result.size());
  return result;
}

}  // namespace bilog::test
