#include "composedatasource.h"
#include "mixaudio.h"
#include <QDebug>
#include <algorithm>

ComposeDataSource::ComposeDataSource(int64_t frame_size,
                                     AVSampleFormat sample_format)
    : DataSource(frame_size), m_sample_format(sample_format) {
  assert(sample_format == AV_SAMPLE_FMT_FLT);
  m_mix_audio = std::make_shared<MixAudio>();
}

void ComposeDataSource::addDataSource(std::shared_ptr<DataSource> data_source) {
  if (!data_source) {
    return;
  }
  m_data_sources.push_back(data_source);
}

int64_t ComposeDataSource::bytesAvailable() const {
  if (m_data_sources.empty()) {
    return 0;
  }
  int64_t max_bytes = 0;
  for (const auto &source : m_data_sources) {
    max_bytes = std::max(max_bytes, source->bytesAvailable());
  }
  return max_bytes;
}

int64_t ComposeDataSource::realReadData(uint8_t *data, int64_t max_size) {
  if (!data || max_size <= 0 || m_data_sources.empty()) {
    return 0;
  }

  auto it = m_data_sources.begin();
  int64_t read_size = 0;
  while (it != m_data_sources.end()) {
    read_size = (*it)->readData(data, max_size);
    ++it;
    if (read_size > 0) {
      break;
    }
  }
  if (read_size <= 0 || it == m_data_sources.end()) {
    return read_size;
  }

  std::vector<uint8_t> temp_buffer(read_size);
  while (it != m_data_sources.end()) {
    auto size = (*it)->readData(temp_buffer.data(), read_size);
    ++it;
    if (size != read_size) {
      continue;
    }
    m_mix_audio->mix<float>(data, temp_buffer.data(), read_size,
                            MixAudio::MIX_MODE_SOFT_CLIP);

  }
  return read_size;
}

bool ComposeDataSource::realIsEnd() const {
  if (m_data_sources.empty()) {
    return true;
  }
  for (const auto &source : m_data_sources) {
    if (!source->isEnd()) {
      return false;
    }
  }
  return true;
}

void ComposeDataSource::realClear() {
  for (auto &source : m_data_sources) {
    if (source) {
      source->clear();
    }
  }
}
