#pragma once

#include <QObject>
#include <QTimer>
#include <filesystem>
#include <memory>

#ifdef _WIN32
#ifdef SOUNDKITS_EXPORTS
#define SOUNDKITS_API __declspec(dllexport)
#else
#define SOUNDKITS_API __declspec(dllimport)
#endif
#else
#define SOUNDKITS_API
#endif

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

class SOUNDKITS_API AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();

  static  AudioInfo fetchFullAudioInfo(QString fpath, int fetch_samples_num);
  void open(const std::vector<QString> &in_fpaths);
  void play();
  void pause();
  void stop();
  bool isPlaying();
  int64_t duration();
  void seek(int64_t time_ms);
  void setVolume(float volume);
  void setVolumeBalance(float balance);
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
  std::shared_ptr<AudioEffectsFilter> m_effects_filter;

  std::vector<std::shared_ptr<AudioDecoder>> m_audio_decoders;
  std::vector<std::shared_ptr<DecodeQueue>> m_decode_queues;

  std::shared_ptr<ComposeDataSource> m_compose_source;
  std::vector<std::filesystem::path> m_in_fpaths;
  std::atomic<bool> m_stoped;

  QTimer m_update_timer;
  int64_t m_max_duration_ms;
};
