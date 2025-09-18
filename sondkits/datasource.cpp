#include "datasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <cstring>


DataSource::DataSource(std::shared_ptr<AudioFilter> audio_filter)
    : m_audio_filter(audio_filter) {}

int64_t DataSource::readData(uint8_t *data, int64_t size) {
  auto r = realReadData(data, size);
  if (m_audio_filter) {
    m_audio_filter->process(data, &r);
  }
  return r;
}

DecodeDataSource::DecodeDataSource(
    std::shared_ptr<AudioFilter> audio_filter,
    std::shared_ptr<DecodeQueue> decode_queue)
    : DataSource(audio_filter), m_decode_queue(decode_queue) {

}

int64_t DecodeDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  if (!data || maxlen <= 0) {
    return 0;
  }
  return m_decode_queue->readData(reinterpret_cast<uint8_t *>(data), maxlen);
}

bool DecodeDataSource::isEnd() const { return m_decode_queue->canRead(); }

void DecodeDataSource::open() { m_decode_queue->start(); }

int64_t DecodeDataSource::bytesAvailable() const {
  return m_decode_queue->bytesAvailable();
}

/******* ******************************************************/

FileDataSource::FileDataSource(
    std::shared_ptr<AudioFilter> audio_filter,
    const std::string &file_path)
    : DataSource(audio_filter), m_file(file_path.c_str()) {

}

int64_t FileDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  return m_file.read(reinterpret_cast<char *>(data), maxlen);
}

bool FileDataSource::isEnd() const { return m_file.atEnd(); }


int64_t FileDataSource::bytesAvailable() const { return m_file.bytesAvailable(); }

void FileDataSource::open() { m_file.open(QIODevice::ReadOnly); }


/******* ******************************************************/

MemoryDataSource::MemoryDataSource(
    std::shared_ptr<AudioFilter> audio_filter,
    char *data, int size)
    : DataSource(audio_filter), m_data(data), m_size(size), m_pos(0) {

}

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