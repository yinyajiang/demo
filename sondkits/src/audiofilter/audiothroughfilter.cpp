#include "audiothroughfilter.h"

AudioThroughFilter::AudioThroughFilter(int64_t hope_process_size,
                                       bool auto_fill_in)
    : AudioThroughFilter(hope_process_size, nullptr, auto_fill_in) {}

AudioThroughFilter::AudioThroughFilter(
    int64_t hope_process_size,
    std::function<void(uint8_t *data, int64_t size)> real_process,
    bool auto_fill_in)
    : m_hope_process_size(hope_process_size), m_real_process(real_process),
      m_auto_fill_in(auto_fill_in) {
  m_used_cache_size = 0;
}

AudioThroughFilter::~AudioThroughFilter() {}

FilterProcessResult AudioThroughFilter::process(uint8_t *input_data,
                                                int64_t *input_size) {
  auto data = input_data;
  auto total = m_used_cache_size + *input_size;
  auto count = total / m_hope_process_size;
  auto remain = total % m_hope_process_size;
  for (int i = 0; i < count; ++i) {
    if (m_used_cache_size > 0) {
      memcpy(m_cache.data() + m_used_cache_size, data,
             m_hope_process_size - m_used_cache_size);
      data += m_hope_process_size - m_used_cache_size;
      realProcess(m_cache.data(), m_hope_process_size);
      m_used_cache_size = 0;
    } else {
      realProcess(data, m_hope_process_size);
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

int64_t AudioThroughFilter::flushRemaining() { return m_used_cache_size; }

void AudioThroughFilter::reciveRemaining(uint8_t *, int64_t *) {
  if (m_used_cache_size == 0) {
    return;
  }
  if (m_auto_fill_in) {
    memset(m_cache.data() + m_used_cache_size, 0,
           m_cache.size() - m_used_cache_size);
    m_used_cache_size = m_cache.size();
  }
  realProcess(m_cache.data(), m_hope_process_size);
  m_used_cache_size = 0;
}

void AudioThroughFilter::realProcess(uint8_t *data, int64_t size) {
  if (m_real_process) {
    m_real_process(data, size);
  }
}