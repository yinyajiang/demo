
#pragma once
#include <QAudioFormat>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>
#include "decodequeue.h"
#include <QFile>


class PCMDataSource : public QIODevice {
    Q_OBJECT
  public:
    PCMDataSource(std::shared_ptr<DecodeQueue> decode_queue, QObject *parent = nullptr);
  
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
  
  protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
  
  private:
    std::shared_ptr<DecodeQueue> m_decode_queue;
  };
  
  
  class PCMDataSourceFile : public QIODevice {
    Q_OBJECT
  public:
    PCMDataSourceFile(const std::string &file_path, QObject *parent = nullptr);
  
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
  
  protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
  
  private:
    QFile m_file;
  };
  
  class PCMDataSourceMemory : public QIODevice {
    Q_OBJECT
  public:
    PCMDataSourceMemory(char *data, int size, QObject *parent = nullptr);
  
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