
#pragma once
#include "audiofilter.h"
#include <memory>
#include <vector>

class DataSource {
public:
  DataSource(std::shared_ptr<AudioFilter> audio_filter, int64_t frame_size);
  virtual ~DataSource() = default;
  virtual void open() = 0;
  virtual void close() = 0;
  virtual int64_t bytesAvailable() const = 0;
  int64_t readData(uint8_t *data, int64_t size);
  // todo: filter is end ?
  bool isEnd();

protected:
  virtual int64_t realReadData(uint8_t *data, int64_t size) = 0;
  virtual bool realIsEnd() const = 0;

  FilterProcessResult filterProcess(int start_filter_index, uint8_t *data,
                                    int64_t *size);
  FilterProcessResult filterFlushReciveRemaining(int start_filter_index,
                                                 uint8_t *data, int64_t *size);
  bool filterIsFlushed(int start_filter_index);

private:
  std::shared_ptr<AudioFilter> m_audio_filter;

  std::vector<std::shared_ptr<AudioFilter>> m_audio_filters;
  const int64_t m_frame_size;
};
