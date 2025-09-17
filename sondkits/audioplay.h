#pragma once
#include "decodequeue.h"
#include "pcmdatasource.h"
#include <QAudioFormat>
#include <QFile>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <memory>

class AudioPlay : public QObject {
  Q_OBJECT
public:
  explicit AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<PCMDataSource> decode_queue,
                     QObject *parent = nullptr);
  ~AudioPlay();

  void play();
  void stop();
  void pause();
  bool is_playing();
  //[0.0, 1.0]
  void set_volume(float volume);
  float volume();

protected slots:
  void on_state_changed();

private:
  QAudioFormat m_audio_format;
  std::unique_ptr<QAudioSink> m_audio_sink;
  std::shared_ptr<PCMDataSource> m_pcm_data_source;
  float m_volume;
  float m_balance;
};
