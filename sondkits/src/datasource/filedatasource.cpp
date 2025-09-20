#include "filedatasource.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

FileDataSource::FileDataSource(std::shared_ptr<AudioFilter> audio_filter,
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
