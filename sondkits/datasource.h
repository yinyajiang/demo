
#pragma once
#include "decodequeue.h"
#include <QAudioFormat>
#include <QFile>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>
#include "audiofilter.h"

class DataSource{
public:
  DataSource(std::shared_ptr<AudioFilter> audio_filter);
  virtual ~DataSource() = default;
  virtual void open() = 0;
  virtual bool isEnd() const = 0;
  virtual int64_t bytesAvailable() const = 0;
  int64_t readData(uint8_t *data, int64_t size);

protected:
  virtual int64_t realReadData(uint8_t *data, int64_t size) = 0;
private:
  std::shared_ptr<AudioFilter> m_audio_filter;
};

class DecodeDataSource : public DataSource {
public:
  DecodeDataSource(std::shared_ptr<AudioFilter> audio_filter, std::shared_ptr<DecodeQueue> decode_queue);

  void open() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  std::shared_ptr<DecodeQueue> m_decode_queue;
};

class FileDataSource : public DataSource {
public:
  FileDataSource(
      std::shared_ptr<AudioFilter> audio_filter,
      const std::string &file_path);

  void open() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;
private:
  QFile m_file;
};

class MemoryDataSource : public DataSource {
public:
  MemoryDataSource(
      std::shared_ptr<AudioFilter> audio_filter,
      char *data, int size);

  void open() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  char *m_data;
  int m_size;
  int m_pos;
};