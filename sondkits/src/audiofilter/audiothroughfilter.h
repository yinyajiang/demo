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

  void reciveRemaining(uint8_t *, int64_t *) override;
  void clear() override{};

  int64_t hopeProcessSize() const { return m_hope_process_size; }


protected:
  virtual void throughSink(uint8_t *data, int64_t size);

protected:
  int64_t realFlushRemaining() override;
  FilterProcessResult realProcess(uint8_t *data, int64_t *size) override;

private:
  std::function<void(uint8_t *data, int64_t size)> m_alter_sink_fun;
  const int64_t m_hope_process_size;
  std::vector<uint8_t> m_cache;
  int64_t m_used_cache_size;
  bool m_auto_fill_in;
};