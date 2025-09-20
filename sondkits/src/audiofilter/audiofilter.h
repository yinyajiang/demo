#pragma once

#include <cstdint>

enum FilterProcessResult {
  AUDIO_PROCESS_RESULT_SUCCESS,
  AUDIO_PROCESS_RESULT_AGAIN,
  AUDIO_PROCESS_RESULT_ERROR,
};

class AudioFilter {
public:
  virtual ~AudioFilter() = default;
  virtual FilterProcessResult process(uint8_t *data, int64_t *size) = 0;
  virtual int64_t flushRemaining() = 0;
  virtual void reciveRemaining(uint8_t *data, int64_t *size) = 0;
};
