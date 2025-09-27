#pragma once

#include "defexports.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <filesystem>
#include <memory>
#include <vector>

struct AudioInfo {
  float bpm;
  int key;
  QString key_string;
  int channels;
  int sample_rate;
  int duration_seconds;
  QString sample_format;
  int consume_time_ms;
  std::vector<float> samples;
};

class AudioPlay;
class AudioEffectsFilter;
class AudioDecoder;
class DecodeQueue;
class DataSource;
class ComposeDataSource;

class SONDKITS_API AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();

  static AudioInfo fetchFullAudioInfo(QString fpath, int fetch_samples_num);
  void open(const std::vector<QString> &in_fpaths);
  void play();
  void pause();
  void stop();
  bool isPlaying();
  int64_t duration();
  void seek(int64_t time_ms);
  void setVolume(int stream_index, float volume);
  void setVolumeBalance(int stream_index, float balance);
  void setTempo(float tempo);
  void setSemitone(int semitone);

signals:
  void signalTimeProgress(int64_t time_seconds);
  void signalPlayFinished();

private slots:
  void onUpdateTimerTimeout();
  void onStateChanged(int state);

private:
  std::unique_ptr<AudioPlay> m_audio_play;
  std::shared_ptr<AudioEffectsFilter> m_com_effects_filter;
  std::vector<std::shared_ptr<AudioEffectsFilter>> m_streams_effects_filters;

  std::vector<std::shared_ptr<AudioDecoder>> m_decoders;
  std::vector<std::shared_ptr<DecodeQueue>> m_decode_queues;

  std::shared_ptr<ComposeDataSource> m_compose_source;
  std::vector<std::filesystem::path> m_in_fpaths;
  std::atomic<bool> m_stoped;

  QTimer m_update_timer;
  int64_t m_max_duration_ms;
};
