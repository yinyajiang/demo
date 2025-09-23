#pragma once
#include "datasource.h"
#include <QAudioFormat>
#include <QFile>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

class PCMDataSourceDevice;
class AudioPlay : public QObject {
  Q_OBJECT
public:
  explicit AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<DataSource> decode_queue,
                     QObject *parent = nullptr);
  ~AudioPlay();

  void play();
  void stop();
  void pause();
  bool isPlaying();
  void setSinkVoulme(float volume);
  float sinkVolume();
  void saveAsPCMFile(const std::filesystem::path &file_path);

  void setPlayedPositionMs(int64_t position_ms);
  int64_t getPlayedPositionMs() const;

signals:
  void signalStateChanged(QAudio::State state);

protected slots:
  void onStateChanged();

private:
  QAudioFormat m_audio_format;
  std::unique_ptr<QAudioSink> m_audio_sink;
  std::shared_ptr<PCMDataSourceDevice> m_pcm_source;
  float m_volume;
  float m_balance;
};

class PCMDataSourceDevice : public QIODevice {
  Q_OBJECT
public:
  PCMDataSourceDevice(std::shared_ptr<DataSource> data_source,
                      QObject *parent = nullptr);
  virtual ~PCMDataSourceDevice() = default;
  qint64 readData(char *data, qint64 size) override;
  bool atEnd() const override;
  qint64 bytesAvailable() const override;

  qint64 writeData(const char *, qint64) override;

  void setIODevicePlayedBytes(qint64 bytes);
  qint64 getIOdevicePlayedBytes() const;

private:
  std::shared_ptr<DataSource> m_data_source;
  std::atomic<qint64> m_iodevice_played_bytes; // 总共读取的字节数
};