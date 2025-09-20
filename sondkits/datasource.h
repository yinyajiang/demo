
#pragma once
#include "audiofilter.h"
#include "decodequeue.h"
#include <QAudioFormat>
#include <QFile>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>

class DataSource {
public:
  DataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
             int64_t frame_size);
  virtual ~DataSource() = default;
  virtual void open() = 0;
  virtual void close() = 0;
  virtual bool isEnd() const = 0;
  virtual int64_t bytesAvailable() const = 0;
  int64_t readData(uint8_t *data, int64_t size);

protected:
  virtual int64_t realReadData(uint8_t *data, int64_t size) = 0;

private:
  std::shared_ptr<AudioEffectsFilter> m_audio_filter;
  const int64_t m_frame_size;
};

class DecodeDataSource : public DataSource {
public:
  DecodeDataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
                   int64_t frame_size,
                   std::shared_ptr<DecodeQueue> decode_queue);

  void open() override;
  void close() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  std::shared_ptr<DecodeQueue> m_decode_queue;
};

class CustomDataSource : public DataSource {
public:
  CustomDataSource(
      std::shared_ptr<AudioEffectsFilter> audio_filter, int64_t frame_size,
      std::function<int64_t(uint8_t *data, int64_t size)> read_data_func,
      std::function<bool()> is_end_func,
      std::function<int64_t()> bytes_available_func = nullptr);

  void open() override{};
  void close() override{};
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  std::function<int64_t(uint8_t *data, int64_t size)> m_read_data_func;
  std::function<bool()> m_is_end_func;
  std::function<int64_t()> m_bytes_available_func;
};

class FileDataSource : public DataSource {
public:
  FileDataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
                 int64_t frame_size, const std::string &file_path);

  void open() override;
  void close() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  QFile m_file;
};

class MemoryDataSource : public DataSource {
public:
  MemoryDataSource(std::shared_ptr<AudioEffectsFilter> audio_filter,
                   int64_t frame_size, char *data, int size);

  void open() override;
  void close() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  char *m_data;
  int m_size;
  int m_pos;
};