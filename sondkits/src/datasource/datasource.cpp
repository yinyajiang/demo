#include "datasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

DataSource::DataSource(std::shared_ptr<AudioFilter> audio_filter,
                       int64_t frame_size)
    : m_audio_filter(audio_filter), m_frame_size(frame_size) {}

int64_t DataSource::readData(uint8_t *data, int64_t max_size) {
  // 确保size 是每一帧的倍数
  if (max_size % m_frame_size != 0) {
    max_size = max_size / m_frame_size * m_frame_size;
  }

  int64_t r = 0;
  while (1) {
    r = realReadData(data, max_size);
    if (m_audio_filter) {
      if (r == 0) {
        auto remain_size = m_audio_filter->flushRemaining();
        r = std::min(remain_size, max_size);
        m_audio_filter->reciveRemaining(data, &r);
      } else {
        auto result = m_audio_filter->process(data, &r);
        if (result == AUDIO_PROCESS_RESULT_AGAIN) {
          continue;
        }
        if (result != AUDIO_PROCESS_RESULT_SUCCESS) {
          return -1;
        }
      }
    }
    break;
  }

  return r;
}

FilterProcessResult DataSource::filterProcess(int start_filter_index,
                                              uint8_t *data, int64_t *size) {
  if (start_filter_index >= m_audio_filters.size()) {
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }
  for (auto filter_index = start_filter_index;
       filter_index < m_audio_filters.size(); ++filter_index) {
    auto &filter = m_audio_filters[filter_index];
    auto result = filter->process(data, size);
    if (result != AUDIO_PROCESS_RESULT_SUCCESS) {
      return result;
    }
  }
  return AUDIO_PROCESS_RESULT_SUCCESS;
}

FilterProcessResult
DataSource::filterFlushReciveRemaining(int start_filter_index, uint8_t *data,
                                       int64_t *size) {
  if (start_filter_index >= m_audio_filters.size()) {
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }

  auto &filter = m_audio_filters[start_filter_index];
  auto remain = filter->flushRemaining();
  if (remain == 0) {
    return filterFlushReciveRemaining(start_filter_index + 1, data, size);
  }
  filter->reciveRemaining(data, size);
  remain = *size;
  if (remain == 0) {
    return filterFlushReciveRemaining(start_filter_index + 1, data, size);
  }
  if (start_filter_index == m_audio_filters.size() - 1) { // 最后一个
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }
  // 有数据，交给下一个filter处理
  auto next_filter = start_filter_index + 1;
  auto result = filterProcess(next_filter, data, size);
  if (result == AUDIO_PROCESS_RESULT_SUCCESS ||
      result == AUDIO_PROCESS_RESULT_ERROR) {
    return result;
  }
  return filterFlushReciveRemaining(next_filter, data, size);
}