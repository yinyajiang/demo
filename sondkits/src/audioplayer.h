#pragma once

#include "audioinfo.h"
#include <QObject>
#include <filesystem>
#include <memory>

class AudioPlay;
class AudioEffectsFilter;
class AudioDecoder;
class DecodeQueue;
class DataSource;
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
  void seek(int64_t time_ms);
  void setVolume(float volume);
  void setVolumeBalance(float balance);
  void setTempo(float tempo);
  void setSemitone(int semitone);
signals:
  void signalTimeProgress(int64_t time_seconds);
  void signalPlayFinished();

private:
  std::unique_ptr<AudioPlay> m_audio_play;
  std::shared_ptr<AudioEffectsFilter> m_effects_filter;
  std::shared_ptr<AudioDecoder> m_audio_decoder;
  std::shared_ptr<FetchAudioInfo> m_audio_info_fetch;
  std::shared_ptr<DecodeQueue> m_decode_queue;
  std::shared_ptr<DataSource> m_data_source;
  std::filesystem::path m_in_fpath;
  std::atomic<bool> m_stoped;
};