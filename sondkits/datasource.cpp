#include "datasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <cstring>

DataSource::DataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
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

DecodeDataSource::DecodeDataSource(
    std::shared_ptr<AudioEffectsFilter> audio_filter, int64_t frame_size,
    std::shared_ptr<DecodeQueue> decode_queue)
    : DataSource(audio_filter, frame_size), m_decode_queue(decode_queue) {}

int64_t DecodeDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  if (!data || maxlen <= 0) {
    return 0;
  }
  return m_decode_queue->readData(reinterpret_cast<uint8_t *>(data), maxlen);
}

bool DecodeDataSource::isEnd() const { return m_decode_queue->canRead(); }

void DecodeDataSource::open() { m_decode_queue->start(); }

void DecodeDataSource::close() { m_decode_queue->stop(); }

int64_t DecodeDataSource::bytesAvailable() const {
  return m_decode_queue->bytesAvailable();
}

/******* ******************************************************/

FileDataSource::FileDataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
                               int64_t frame_size, const std::string &file_path)
    : DataSource(audio_filter, frame_size), m_file(file_path.c_str()) {}

int64_t FileDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  return m_file.read(reinterpret_cast<char *>(data), maxlen);
}

bool FileDataSource::isEnd() const { return m_file.atEnd(); }

int64_t FileDataSource::bytesAvailable() const {
  return m_file.bytesAvailable();
}

void FileDataSource::open() { m_file.open(QIODevice::ReadOnly); }

void FileDataSource::close() { m_file.close(); }

/******* ******************************************************/

MemoryDataSource::MemoryDataSource(
    std::shared_ptr<AudioEffectsFilter> audio_filter, int64_t frame_size,
    char *data, int size)
    : DataSource(audio_filter, frame_size), m_data(data), m_size(size),
      m_pos(0) {}

int64_t MemoryDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  if (!data || maxlen <= 0) {
    return 0;
  }
  if (m_size <= m_pos) {
    return -1;
  }
  auto len = std::min(static_cast<int>(maxlen), m_size - m_pos);
  memcpy(data, m_data + m_pos, len);
  m_pos += len;
  return len;
}

bool MemoryDataSource::isEnd() const { return m_pos >= m_size; }

int64_t MemoryDataSource::bytesAvailable() const { return m_size - m_pos; }

void MemoryDataSource::open() { m_pos = 0; }

void MemoryDataSource::close() { m_pos = 0; }

CustomDataSource::CustomDataSource(
    std::shared_ptr<AudioEffectsFilter> audio_filter, int64_t frame_size,
    std::function<int64_t(uint8_t *data, int64_t size)> read_data_func,
    std::function<bool()> is_end_func,
    std::function<int64_t()> bytes_available_func)
    : DataSource(audio_filter, frame_size), m_read_data_func(read_data_func),
      m_is_end_func(is_end_func), m_bytes_available_func(bytes_available_func) {
}

int64_t CustomDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  return m_read_data_func(data, maxlen);
}

bool CustomDataSource::isEnd() const { return m_is_end_func(); }

int64_t CustomDataSource::bytesAvailable() const {
  if (m_bytes_available_func) {
    return m_bytes_available_func();
  }
  return 0;
}