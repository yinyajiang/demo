#pragma once

#include <cstdint>

enum FilterProcessResult {
  AUDIO_PROCESS_RESULT_SUCCESS,
  AUDIO_PROCESS_RESULT_AGAIN,
  AUDIO_PROCESS_RESULT_ERROR,
};

class AudioFilter {
public:
  AudioFilter() : m_flushed(false) {}
  virtual ~AudioFilter() = default;
  FilterProcessResult process(uint8_t *data, int64_t *size) {
    if (data && size && *size) {
      m_flushed = false;
    }
    return realProcess(data, size);
  }
  int64_t flushRemaining() {
    m_flushed = true;
    return realFlushRemaining();
  }
  virtual void reciveRemaining(uint8_t *data, int64_t *size) = 0;
  bool isFlushed() { return m_flushed; }

protected:
  virtual int64_t realFlushRemaining() = 0;
  virtual FilterProcessResult realProcess(uint8_t *data, int64_t *size) = 0;

private:
  bool m_flushed;
};
