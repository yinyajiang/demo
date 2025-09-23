#include "memorydatasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <cstring>

MemoryDataSource::MemoryDataSource(int64_t frame_size, char *data, int size)
    : DataSource(frame_size), m_data(data), m_size(size),
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

bool MemoryDataSource::realIsEnd() const { return m_pos >= m_size; }

int64_t MemoryDataSource::bytesAvailable() const { return m_size - m_pos; }

