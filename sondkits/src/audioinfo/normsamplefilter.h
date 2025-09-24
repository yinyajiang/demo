#pragma once

#include "audiothroughfilter.h"
extern "C" {
#include "libavutil/samplefmt.h"
}

class NormSampleFilter : public AudioThroughFilter {
public:
  NormSampleFilter(int sample_rate, int channels, AVSampleFormat format,
                   int64_t total_duration_seconds, int num_points);
  ~NormSampleFilter();

  std::vector<float> getSamplePoints();

protected:
  void throughSink(uint8_t *data, int64_t size) override;

private:
  int64_t m_num_points;
  int64_t m_num_samples_per_point;
  float m_sum_value;
  int64_t m_sum_count;
  std::vector<float> m_sample_points;
  float m_max_sample_point;
  float m_min_sample_point;
};