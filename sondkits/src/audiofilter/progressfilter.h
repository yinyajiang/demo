#pragma once

#include "audiothroughfilter.h"
#include <chrono>
#include <functional>
extern "C" {
#include "libavutil/samplefmt.h"
}

using ProgressCallback = std::function<void(float)>;
class ProgressFilter : public AudioThroughFilter {
public:
  ProgressFilter(int sample_rate, int channels, AVSampleFormat format,
                 int64_t total_duration_seconds,
                 std::chrono::milliseconds interval);
  ~ProgressFilter();

  void setProgressCallback(ProgressCallback progress_callback);

protected:
  void throughSink(uint8_t *, int64_t size) override;

private:
  int m_size_per_second;
  int64_t m_total_duration_seconds;
  ProgressCallback m_progress_callback;
  float m_through_duration_seconds;
  std::chrono::milliseconds m_interval;
  std::chrono::steady_clock::time_point m_last_time;
};