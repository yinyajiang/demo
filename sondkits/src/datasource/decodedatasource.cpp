#include "decodedatasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

DecodeDataSource::DecodeDataSource(std::shared_ptr<AudioFilter> audio_filter,
                                   int64_t frame_size,
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
