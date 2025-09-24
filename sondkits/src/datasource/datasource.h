
#pragma once
#include "audiofilter.h"
#include <memory>
#include <vector>
#include <chrono>

class DataSource {
public:
  DataSource(int64_t frame_size);
  virtual ~DataSource() = default;
  virtual int64_t bytesAvailable() const = 0;
  int64_t readData(uint8_t *data, int64_t size);
  bool isEnd();
  void consumeAll();
  void addFilter(std::shared_ptr<AudioFilter> filter);
  void abort();
  int64_t frameSize() const;
  virtual void clear();
  bool waitHasData(std::chrono::milliseconds timeout);

protected:
  virtual void realClear() = 0;
  virtual int64_t realReadData(uint8_t *data, int64_t size) = 0;
  virtual bool realIsEnd() const = 0;

  FilterProcessResult filterProcess(int start_filter_index, uint8_t *data,
                                    int64_t *size);
  FilterProcessResult filterFlushReceiveRemaining(int start_filter_index,
                                                 uint8_t *data, int64_t *size);
  bool filterIsFlushed(int start_filter_index);
  int findNoFlushedFilterIndex();

private:
  std::vector<std::shared_ptr<AudioFilter>> m_audio_filters;
  const int64_t m_frame_size;
  bool m_aborted;
};
