#include "audiothroughfilter.h"

AudioThroughFilter::AudioThroughFilter(int64_t hope_process_size,
                                       bool auto_fill_in)
    : AudioThroughFilter(hope_process_size, nullptr, auto_fill_in) {}

AudioThroughFilter::AudioThroughFilter(
    int64_t hope_process_size,
    std::function<void(uint8_t *data, int64_t size)> real_process,
    bool auto_fill_in)
    : AudioFilter(), m_hope_process_size(hope_process_size),
      m_alter_sink_fun(real_process), m_auto_fill_in(auto_fill_in) {
  m_used_cache_size = 0;
}

AudioThroughFilter::~AudioThroughFilter() {}

FilterProcessResult AudioThroughFilter::realProcess(uint8_t *input_data,
                                                    int64_t *input_size) {
  if (!input_data || !input_size || *input_size <= 0 || m_hope_process_size <= 0) {
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }
  auto data = input_data;
  auto total = m_used_cache_size + *input_size;
  auto count = total / m_hope_process_size;
  auto remain = total % m_hope_process_size;
  for (int64_t i = 0; i < count; ++i) {
    if (m_used_cache_size > 0) {
      memcpy(m_cache.data() + m_used_cache_size, data,
             m_hope_process_size - m_used_cache_size);
      data += m_hope_process_size - m_used_cache_size;
      throughSink(m_cache.data(), m_hope_process_size);
      m_used_cache_size = 0;
    } else {
      throughSink(data, m_hope_process_size);
      data += m_hope_process_size;
    }
  }

  if (remain > 0) {
    if (m_cache.empty()) {
      m_cache.resize(m_hope_process_size);
    }
    memcpy(m_cache.data() + m_used_cache_size, data,
           remain - m_used_cache_size);
    m_used_cache_size = remain;
  }
  return AUDIO_PROCESS_RESULT_SUCCESS;
}

int64_t AudioThroughFilter::realFlushRemaining() { return m_used_cache_size; }

void AudioThroughFilter::reciveRemaining(uint8_t *, int64_t *) {
  if (m_used_cache_size == 0) {
    return;
  }
  if (m_auto_fill_in) {
    if (m_cache.empty()) {
      m_cache.resize(m_hope_process_size);
    }
    memset(m_cache.data() + m_used_cache_size, 0,
           m_hope_process_size - m_used_cache_size);
    throughSink(m_cache.data(), m_hope_process_size);
  } else {
    throughSink(m_cache.data(), m_used_cache_size);
  }
  m_used_cache_size = 0;
}

void AudioThroughFilter::throughSink(uint8_t *data, int64_t size) {
  if (m_alter_sink_fun) {
    m_alter_sink_fun(data, size);
  }
}