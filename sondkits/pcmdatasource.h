
#pragma once
#include "decodequeue.h"
#include <QAudioFormat>
#include <QFile>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>

class PCMDataSource : public QIODevice {
  Q_OBJECT
public:
  PCMDataSource(QObject *parent = nullptr) : QIODevice(parent) {}
  virtual ~PCMDataSource() = default;
  virtual void start() = 0;
};

class DecodePCMSource : public PCMDataSource {
  Q_OBJECT
public:
  DecodePCMSource(std::shared_ptr<DecodeQueue> decode_queue,
                  QObject *parent = nullptr);

  void start() override;
  bool atEnd() const override;
  qint64 bytesAvailable() const override;

protected:
  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;

private:
  std::shared_ptr<DecodeQueue> m_decode_queue;
};

class PCMDataSourceFile : public PCMDataSource {
  Q_OBJECT
public:
  PCMDataSourceFile(const std::string &file_path, QObject *parent = nullptr);

  void start() override;
  bool atEnd() const override;
  qint64 bytesAvailable() const override;

protected:
  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;

private:
  QFile m_file;
};

class PCMDataSourceMemory : public PCMDataSource {
  Q_OBJECT
public:
  PCMDataSourceMemory(char *data, int size, QObject *parent = nullptr);

  void start() override;
  bool atEnd() const override;
  qint64 bytesAvailable() const override;

protected:
  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;

private:
  char *m_data;
  int m_size;
  int m_pos;
};