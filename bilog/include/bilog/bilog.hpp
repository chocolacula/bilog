#pragma once

#include <cstddef>

#include "bilog/codec/binary.hpp"
#include "bilog/event.hpp"
#include "bilog/sink/file.hpp"

#ifndef BILOG_ENCODER
#define BILOG_ENCODER BinaryEncoder
#endif
#ifndef BILOG_SINK
#define BILOG_SINK FileSink
#endif

namespace bilog {

/// @brief global static logger class
template <typename EncoderT, typename SinkT>
class Logger {
  static thread_local Buffer<SinkT> buff_;

  EncoderT encoder_;
  SinkT sink_;
  Level min_level_ = Level::kError;

 public:
  using encoder_t = EncoderT;
  using sink_t = SinkT;

  Logger() = default;
  ~Logger() = default;

  Logger(Level min_level, EncoderT&& encoder, SinkT&& sink)
      : min_level_(min_level),  //
        encoder_(std::move(encoder)),
        sink_(std::move(sink)) {}

  Logger& operator=(Logger&& other) noexcept = default;

 private:
  static Logger& instance() {
    static Logger logger;
    return logger;
  }

  Event<EncoderT, SinkT> log(std::initializer_list<std::uint64_t> ids) {
    auto* buff = &buff_;
    buff->set_sink(&sink_);
    return Event<EncoderT, SinkT>(min_level_, &encoder_, buff, &sink_, ids);
  }

  friend void init(Level min_level, EncoderT&& encoder, SinkT&& sink);
  friend Event<EncoderT, SinkT> log(std::initializer_list<std::uint64_t> dont_edit);
};

template <typename EncoderT, typename SinkT>
thread_local Buffer<SinkT> Logger<EncoderT, SinkT>::buff_(SinkT::kBuffCap);

using CurrentLogger = Logger<BILOG_ENCODER, BILOG_SINK>;

inline void init(Level min_level,
                 CurrentLogger::encoder_t&& encoder,
                 CurrentLogger::sink_t&& sink) {

  auto& logger = CurrentLogger::instance();
  logger.min_level_ = min_level;
  logger.encoder_ = std::move(encoder);
  logger.sink_ = std::move(sink);
}

inline Event<CurrentLogger::encoder_t, CurrentLogger::sink_t> log(
    std::initializer_list<std::uint64_t> dont_edit) {
  return CurrentLogger::instance().log(dont_edit);
}

};  // namespace bilog
