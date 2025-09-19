#pragma once

#include <QObject>
#include <filesystem>
#include <memory>


struct AudioInfo {
  float bpm;
  int key;
  int channels;
  int sample_rate;
  int duration_seconds;
  std::string sample_format;
  int consume_time_ms;
};


class AudioPlay;
class AudioFilter;
class AudioDecoder;
class AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();

  AudioInfo fetchAudioInfo();
  void open(const std::filesystem::path &in_fpath);
  void play();
  void pause();
  void stop();
  bool isPlaying();
  int64_t duration();
  int64_t seek(int64_t time_ms);
  void setVolume(float volume);
  void setVolumeBalance(float balance);
  void setTempo(float tempo);
  void setSemitone(int semitone);
signals:
  void signal_update_time(int64_t time_seconds);
  void signal_play_finished();

private:
  float detectBPMUseSoundtouch();
  float detectBPMUseAubio();
private:
  std::unique_ptr<AudioPlay> m_audio_play;
  std::shared_ptr<AudioFilter> m_audio_filter;
  std::shared_ptr<AudioDecoder> m_audio_decoder;
  std::filesystem::path m_in_fpath;
  std::atomic<bool> m_stoped;
};