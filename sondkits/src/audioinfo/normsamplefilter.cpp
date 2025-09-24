#include "normsamplefilter.h"
#include <cassert>
#include <memory>
#include <stdexcept>
extern "C" {
#include "libavutil/samplefmt.h"
}

#define NORM_SAMPLE_FILTER_HOP_SIZE 1024

NormSampleFilter::NormSampleFilter(int sample_rate, int channels,
                                   AVSampleFormat format,
                                   int64_t total_duration_seconds,
                                   int num_points)
    : AudioThroughFilter(true) {
  assert(channels == 1);
  assert(format == AV_SAMPLE_FMT_FLT);
  m_sum_value = 0;
  m_sum_count = 0;
  m_max_sample_point = 0;
  m_min_sample_point = std::numeric_limits<float>::max();

  int64_t total_samples = total_duration_seconds * sample_rate;
  if (num_points <= 0) {
    throw std::invalid_argument("num_points must be positive");
  }
  m_num_samples_per_point = int64_t(double(total_samples) / double(num_points));
  m_num_points = num_points;
  setHopeProcessSize(NORM_SAMPLE_FILTER_HOP_SIZE * sizeof(float));
}

NormSampleFilter::~NormSampleFilter() {
  // 清理资源
}

void NormSampleFilter::throughSink(uint8_t *data, int64_t size) {
  if(m_sample_points.size() >= m_num_points) {
    return;
  }
  float *float_data = reinterpret_cast<float *>(data);
  for (int i = 0; i < size / sizeof(float); i++) {
    ++m_sum_count;
    m_sum_value += std::abs(float_data[i]);
    if (m_sum_count % m_num_samples_per_point == 0) {
      float v = m_sum_value / m_num_samples_per_point;
      m_sample_points.push_back(v);
      m_max_sample_point = std::max(m_max_sample_point, v);
      m_min_sample_point = std::min(m_min_sample_point, v);

      m_sum_value = 0;
    }
  }
}

std::vector<float> NormSampleFilter::getSamplePoints() const {
  std::vector<float> result = m_sample_points;
  if(result.size() < static_cast<size_t>(m_num_points)) {
    result.resize(m_num_points, 0.0f);
  }
  
  float diff = m_max_sample_point - m_min_sample_point;
  if(diff == 0) {
    return result;
  }
  for(size_t i = 0; i < result.size(); i++) {
    result[i] = (result[i] - m_min_sample_point) / diff;
  }
  return result;
}