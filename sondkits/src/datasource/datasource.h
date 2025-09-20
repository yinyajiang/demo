
#pragma once
#include "audiofilter.h"
#include <memory>

class DataSource {
public:
  DataSource(std::shared_ptr<AudioFilter> audio_filter, int64_t frame_size);
  virtual ~DataSource() = default;
  virtual void open() = 0;
  virtual void close() = 0;
  virtual bool isEnd() const = 0;
  virtual int64_t bytesAvailable() const = 0;
  int64_t readData(uint8_t *data, int64_t size);

protected:
  virtual int64_t realReadData(uint8_t *data, int64_t size) = 0;

private:
  std::shared_ptr<AudioFilter> m_audio_filter;
  const int64_t m_frame_size;
};
