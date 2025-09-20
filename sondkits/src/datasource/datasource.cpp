#include "datasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

DataSource::DataSource(std::shared_ptr<AudioFilter> audio_filter,
                       int64_t frame_size)
    : m_audio_filter(audio_filter), m_frame_size(frame_size) {}

int64_t DataSource::readData(uint8_t *data, int64_t size) {
  // 确保size 是每一帧的倍数
  if (size % m_frame_size != 0) {
    size = size / m_frame_size * m_frame_size;
  }

  int64_t r = 0;
  while (1) {
    r = realReadData(data, size);
    if (m_audio_filter) {
      if (r == 0) {
        auto remain_size = m_audio_filter->flushRemaining();
        r = std::min(remain_size, size);
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
