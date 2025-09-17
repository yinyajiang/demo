#pragma once

#include <QObject>
#include <filesystem>
#include <memory>

class AudioPlay;
class AudioDecoder;
class DecodeQueue;
class AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();
  void open(const std::filesystem::path &in_fpath);
  void play();
  void stop();
  bool is_playing();

private:
  std::unique_ptr<AudioPlay> m_audio_play;
};