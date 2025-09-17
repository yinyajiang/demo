#include "pcmdatasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <cstring>

DecodePCMSource::DecodePCMSource(std::shared_ptr<DecodeQueue> decode_queue,
                                 QObject *parent)
    : PCMDataSource(parent), m_decode_queue(decode_queue) {
  open(QIODevice::ReadOnly);
}

qint64 DecodePCMSource::readData(char *data, qint64 maxlen) {
  if (!data || maxlen <= 0) {
    return 0;
  }
  return m_decode_queue->read_data(reinterpret_cast<uint8_t *>(data), maxlen);
}

qint64 DecodePCMSource::writeData(const char *data, qint64 len) { return -1; }

bool DecodePCMSource::atEnd() const { return m_decode_queue->read_ended(); }

void DecodePCMSource::start() { m_decode_queue->start(); }

qint64 DecodePCMSource::bytesAvailable() const {
  return m_decode_queue->bytes_available();
}

PCMDataSourceFile::PCMDataSourceFile(const std::string &file_path,
                                     QObject *parent)
    : PCMDataSource(parent), m_file(file_path.c_str()) {
  open(QIODevice::ReadOnly);
}

qint64 PCMDataSourceFile::readData(char *data, qint64 maxlen) {
  return m_file.read(data, maxlen);
}

qint64 PCMDataSourceFile::writeData(const char *data, qint64 len) { return -1; }

bool PCMDataSourceFile::atEnd() const { return m_file.atEnd(); }

qint64 PCMDataSourceFile::bytesAvailable() const {
  return m_file.bytesAvailable();
}

void PCMDataSourceFile::start() { m_file.open(QIODevice::ReadOnly); }

PCMDataSourceMemory::PCMDataSourceMemory(char *data, int size, QObject *parent)
    : PCMDataSource(parent), m_data(data), m_size(size), m_pos(0) {
  open(QIODevice::ReadOnly);
}

qint64 PCMDataSourceMemory::readData(char *data, qint64 maxlen) {
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

qint64 PCMDataSourceMemory::writeData(const char *data, qint64 len) {
  return -1;
}

bool PCMDataSourceMemory::atEnd() const { return m_pos >= m_size; }

qint64 PCMDataSourceMemory::bytesAvailable() const { return m_size - m_pos; }

void PCMDataSourceMemory::start() { m_pos = 0; }