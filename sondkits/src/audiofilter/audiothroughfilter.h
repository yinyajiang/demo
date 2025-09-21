#pragma once

#include "audiofilter.h"
#include <functional>
#include <vector>

class AudioThroughFilter : public AudioFilter {
public:
  AudioThroughFilter(int64_t hope_process_size, bool auto_fill_in = false);
  AudioThroughFilter(
      int64_t hope_process_size,
      std::function<void(uint8_t *data, int64_t size)> real_process,
      bool auto_fill_in = false);
  ~AudioThroughFilter();

  FilterProcessResult process(uint8_t *data, int64_t *size) override;

  int64_t flushRemaining() override;

  void reciveRemaining(uint8_t *, int64_t *) override;

protected:
  virtual void realProcess(uint8_t *data, int64_t size);

private:
  std::function<void(uint8_t *data, int64_t size)> m_real_process;
  const int64_t m_hope_process_size;
  std::vector<uint8_t> m_cache;
  int64_t m_used_cache_size;
  bool m_auto_fill_in;
};