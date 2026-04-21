#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "bilog/level.hpp"
#include "bilog/sink/buffer.hpp"
#include "bilog/tag.hpp"

namespace bilog {

template <typename EncoderT, typename SinkT>
class EventWriter;

template <typename EncoderT, typename SinkT>
class Event {
  Level min_level_;
  EncoderT* encoder_;
  Buffer<SinkT>* buff_;
  SinkT* sink_;

  const std::uint64_t* id_;
  const std::uint64_t* id_end_;

 public:
  Event(Level min_level,
        EncoderT* encoder,
        Buffer<SinkT>* buff,
        SinkT* sink,
        std::initializer_list<std::uint64_t> ids)
      : min_level_(min_level),
        encoder_(encoder),
        buff_(buff),
        sink_(sink),
        id_(ids.begin()),
        id_end_(ids.end()) {}

  EventWriter<EncoderT, SinkT> trace(const char* str) {
    if (min_level_ > Level::kTrace) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kTrace, str);
  }
  EventWriter<EncoderT, SinkT> debug(const char* str) {
    if (min_level_ > Level::kDebug) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kDebug, str);
  }
  EventWriter<EncoderT, SinkT> info(const char* str) {
    if (min_level_ > Level::kInfo) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kInfo, str);
  }
  EventWriter<EncoderT, SinkT> warn(const char* str) {
    if (min_level_ > Level::kWarn) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kWarn, str);
  }
  EventWriter<EncoderT, SinkT> error(const char* str) {
    if (min_level_ > Level::kError) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kError, str);
  }
  EventWriter<EncoderT, SinkT> fatal(const char* str) {
    if (min_level_ > Level::kFatal) {
      return EventWriter<EncoderT, SinkT>(nullptr);
    }
    return begin(Level::kFatal, str);
  }

 private:
  EventWriter<EncoderT, SinkT> begin(Level lvl, const char* str) {
    // pair 1: [event_id, level]
    encoder_->encode_pair(buff_, sink_, next_id(), lvl.to_byte());
    // pair 2: [message_tag, 0_placeholder]
    encoder_->encode_pair(buff_, sink_, Tag(next_id(), str), 0);
    return EventWriter<EncoderT, SinkT>(this);
  }

  std::uint64_t next_id() {
    assert(id_ < id_end_);
    if (id_ < id_end_) [[likely]] {
      return *(id_++);
    } else {
      return 0;
    }
  }

  friend EventWriter<EncoderT, SinkT>;
};

template <typename EncoderT, typename SinkT>
class EventWriter {
  Event<EncoderT, SinkT>* event_;

 public:
  explicit EventWriter(Event<EncoderT, SinkT>* event) : event_(event) {}

  template <std::integral T>
  EventWriter i(const char* str, T val) {
    if (event_ == nullptr) [[unlikely]] {
      return *this;
    }
    event_->encoder_->encode_pair(event_->buff_,  //
                                  event_->sink_,
                                  Tag(event_->next_id(), str),
                                  val);
    return *this;
  }

  template <std::floating_point T>
  EventWriter f(const char* str, T val) {
    if (event_ == nullptr) [[unlikely]] {
      return *this;
    }
    event_->encoder_->encode_pair(event_->buff_,  //
                                  event_->sink_,
                                  Tag(event_->next_id(), str),
                                  val);
    return *this;
  }

  EventWriter b(const char* str, bool val) {
    if (event_ == nullptr) [[unlikely]] {
      return *this;
    }
    event_->encoder_->encode_pair(event_->buff_,  //
                                  event_->sink_,
                                  Tag(event_->next_id(), str),
                                  val);
    return *this;
  }

  EventWriter s(const char* str, const std::string& val) {
    if (event_ == nullptr) [[unlikely]] {
      return *this;
    }
    event_->encoder_->encode_pair(event_->buff_,  //
                                  event_->sink_,
                                  Tag(event_->next_id(), str),
                                  val);
    return *this;
  }

  EventWriter cs(const char* str, const char* val) {
    if (event_ == nullptr) [[unlikely]] {
      return *this;
    }
    event_->encoder_->encode_pair(event_->buff_,  //
                                  event_->sink_,
                                  Tag(event_->next_id(), str),
                                  Tag(event_->next_id(), val));
    return *this;
  }

  void write() {
    if (event_ == nullptr) [[unlikely]] {
      return;
    }
    event_->encoder_->commit(event_->buff_, event_->sink_);
  }
};

}  // namespace bilog
