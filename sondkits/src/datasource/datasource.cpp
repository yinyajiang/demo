#include "datasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

DataSource::DataSource(int64_t frame_size)
    : m_frame_size(frame_size), m_aborted(false) {}

void DataSource::addFilter(std::shared_ptr<AudioFilter> filter) {
  m_audio_filters.push_back(filter);
}

void DataSource::abort() { m_aborted = true; }

void DataSource::clear() {
  realClear();
  for (auto &filter : m_audio_filters) {
    filter->clear();
  }
}

int64_t DataSource::frameSize() const { return m_frame_size; }

bool DataSource::isEnd() { return m_aborted || (realIsEnd() && filterIsFlushed(0)); }

void DataSource::consumeAll() {
  std::vector<uint8_t> buffer(m_frame_size * 1024);
  while (!isEnd()) {
    while (1) {
      auto r = readData(buffer.data(), buffer.size());
      if (r == 0) {
        break;
      }
    }
  }
}

int64_t DataSource::readData(uint8_t *data, int64_t max_size) {
  // 确保size 是每一帧的倍数
  if (max_size % m_frame_size != 0) {
    max_size = max_size / m_frame_size * m_frame_size;
  }

  int64_t r = 0;
  while (!m_aborted) {
    r = realReadData(data, max_size);
    if (!m_audio_filters.empty()) {
      if (r == 0) {
        r = max_size;
        filterFlushReceiveRemaining(findNoFlushedFilterIndex(),data, &r);
      } else {
        auto result = filterProcess(findNoFlushedFilterIndex(),data, &r);
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
DataSource::filterFlushReceiveRemaining(int start_filter_index, uint8_t *data,
                                        int64_t *size) {
  if (m_aborted) {
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }
  int64_t buffer_size = *size;
  *size = 0; // output size
  if (start_filter_index >= m_audio_filters.size() || start_filter_index < 0) {
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }

  auto &filter = m_audio_filters[start_filter_index];
  auto remain = filter->flushRemaining();
  if (remain == 0) {
    *size = buffer_size;
    return filterFlushReceiveRemaining(start_filter_index + 1, data, size);
  }

  *size = buffer_size;
  filter->reciveRemaining(data, size);
  if (*size == 0) {
    *size = buffer_size;
    return filterFlushReceiveRemaining(start_filter_index + 1, data, size);
  }
  if (start_filter_index == m_audio_filters.size() - 1) { // 最后一个
    return AUDIO_PROCESS_RESULT_SUCCESS;
  }
  // 有数据，交给下一个filter处理
  auto next_filter = start_filter_index + 1;
  auto result = filterProcess(next_filter, data, size);
  if (result == AUDIO_PROCESS_RESULT_AGAIN) {
    *size = buffer_size;
    return filterFlushReceiveRemaining(next_filter, data, size);
  }
  return result;
}

bool DataSource::filterIsFlushed(int start_filter_index) {
  if (start_filter_index >= m_audio_filters.size() || start_filter_index < 0) {
    return true;
  }
  for (auto filter_index = start_filter_index;
       filter_index < m_audio_filters.size(); ++filter_index) {
    auto &filter = m_audio_filters[filter_index];
    if (!filter->isFlushed()) {
      return false;
    }
  }
  return true;
}

int DataSource::findNoFlushedFilterIndex() {
  for (auto filter_index = 0; filter_index < m_audio_filters.size(); ++filter_index) {
    auto &filter = m_audio_filters[filter_index];
    if (!filter->isFlushed()) {
      return filter_index;
    }
  }
  return -1;
}
