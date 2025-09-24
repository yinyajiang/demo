#include "progressfilter.h"

ProgressFilter::ProgressFilter(int sample_rate, int channels,
                               AVSampleFormat format,
                               int64_t total_duration_seconds,
                               std::chrono::milliseconds interval)
    : AudioThroughFilter(false),
      m_total_duration_seconds(total_duration_seconds) {

  m_size_per_second = sample_rate * channels * av_get_bytes_per_sample(format);
  m_through_duration_seconds = 0.0f;
  m_interval = interval;
  m_last_time = std::chrono::steady_clock::time_point{};
}

ProgressFilter::~ProgressFilter() {}

void ProgressFilter::setProgressCallback(ProgressCallback progress_callback) {
  m_progress_callback = progress_callback;
}

void ProgressFilter::throughSink(uint8_t *, int64_t size) {
  if (!m_progress_callback || m_size_per_second == 0 ||
      m_total_duration_seconds <= 0) {
    return;
  }

  m_through_duration_seconds +=
      static_cast<float>(size) / static_cast<float>(m_size_per_second);
  auto now = std::chrono::steady_clock::now();
  if (m_last_time.time_since_epoch().count() == 0 ||
      now - m_last_time > m_interval) {
    m_progress_callback(
        std::min(m_through_duration_seconds / m_total_duration_seconds, 1.0f));
    m_last_time = now;
  }
}