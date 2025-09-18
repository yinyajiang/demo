#pragma once

#include <QObject>
#include <filesystem>
#include <memory>

class AudioPlay;
class AudioFilter;
class AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();
  void open(const std::filesystem::path &in_fpath);
  void play();
  void stop();
  bool isPlaying();
  int64_t duration();
  int64_t seek(int64_t time_ms);
  void setVolume(float volume);
  void setVolumeBalance(float balance);
  void setTempo(float tempo);
signals:
  void signal_update_time(int64_t time_seconds);
  void signal_play_finished();

private:
  std::unique_ptr<AudioPlay> m_audio_play;
  std::shared_ptr<AudioFilter> m_audio_filter;
};