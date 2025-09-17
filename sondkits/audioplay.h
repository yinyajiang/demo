#pragma once
#include <QAudioFormat>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>
#include "decodequeue.h"
#include <QFile>
#include "pcmdatasource.h"


class AudioPlay : public QObject {
  Q_OBJECT
public:
  explicit AudioPlay(QAudioFormat audio_format, std::shared_ptr<DecodeQueue> decode_queue, QObject *parent = nullptr);
  ~AudioPlay();

  void play();
  void stop();
  void pause();
  bool is_playing();
  void input_pcm_data(const uint8_t *data, int size);
  //[0.0, 1.0]
  void set_volume(float volume);
  float volume();

signals:
  void signal_request_more_data(qint64 size);

protected slots:
  void on_state_changed();

private:
  QAudioFormat m_audio_format;
  std::unique_ptr<QAudioSink> m_audio_sink;
  std::shared_ptr<DecodeQueue> m_decode_queue;
  std::unique_ptr<PCMDataSource> m_pcm_data_source;
  float m_volume;
  float m_balance;
};
