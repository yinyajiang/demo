#include "pcmdatasource.h"
#include "audioplay.h"
#include <QDebug>
#include <QtGlobal>
#include <cstring>
#include <QFile>
#include "audiodecoder.h"
#include <fstream>
#include <iostream>

PCMDataSource::PCMDataSource(std::shared_ptr<DecodeQueue> decode_queue,
    QObject *parent)
: QIODevice(parent), m_decode_queue(decode_queue) {
    open(QIODevice::ReadOnly);
}


qint64 PCMDataSource::readData(char *data, qint64 maxlen) {
    if (!data || maxlen <= 0) {
    return 0;
    }
    //计算耗时
    auto start = std::chrono::high_resolution_clock::now();
    auto r = m_decode_queue->read_data(reinterpret_cast<uint8_t *>(data), maxlen);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "### readData duration: " << duration.count() << "us" << std::endl;
    return r;
}

qint64 PCMDataSource::writeData(const char *data, qint64 len) { return -1; }


bool PCMDataSource::atEnd() const { return m_decode_queue->is_empty(); }

qint64 PCMDataSource::bytesAvailable() const {
    return m_decode_queue->bytes_available();
}


PCMDataSourceFile::PCMDataSourceFile(const std::string &file_path, QObject *parent)
: QIODevice(parent), m_file(file_path.c_str()) {
    open(QIODevice::ReadOnly);
    m_file.open(QIODevice::ReadOnly);
}

qint64 PCMDataSourceFile::readData(char *data, qint64 maxlen) {
    if (!data || maxlen <= 0) {
    auto r = m_file.read(data, maxlen);
    return r;
    }
    auto start = std::chrono::high_resolution_clock::now();
    auto r = m_file.read(data, maxlen);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "### read:" << r << std::endl;
    std::cout << "### readData duration: " << duration.count() << "us" << std::endl;
    return r;
}

qint64 PCMDataSourceFile::writeData(const char *data, qint64 len) { return -1; }

bool PCMDataSourceFile::atEnd() const {
    auto b = m_file.atEnd();
    std::cout << "### atEnd : " << b  << std::endl;
    return m_file.atEnd();
}

qint64 PCMDataSourceFile::bytesAvailable() const {
    return m_file.bytesAvailable();
}



PCMDataSourceMemory::PCMDataSourceMemory(char *data, int size, QObject *parent)
: QIODevice(parent), m_data(data), m_size(size), m_pos(0) {
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

qint64 PCMDataSourceMemory::writeData(const char *data, qint64 len) { return -1; }

bool PCMDataSourceMemory::atEnd() const { return m_pos >= m_size; }

qint64 PCMDataSourceMemory::bytesAvailable() const {
    return m_size - m_pos;
}