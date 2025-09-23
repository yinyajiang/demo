#include "filedatasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

FileDataSource::FileDataSource(int64_t frame_size, const std::string &file_path)
    : DataSource(frame_size), m_file(file_path.c_str()) {

      m_file.open(QIODevice::ReadOnly);
    }

FileDataSource::~FileDataSource() {m_file.close();}

int64_t FileDataSource::realReadData(uint8_t *data, int64_t maxlen) {
  return m_file.read(reinterpret_cast<char *>(data), maxlen);
}

bool FileDataSource::realIsEnd() const { return m_file.atEnd(); }

int64_t FileDataSource::bytesAvailable() const {
  return m_file.bytesAvailable();
}



