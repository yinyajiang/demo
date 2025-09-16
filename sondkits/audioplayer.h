#pragma once

#include <QObject>
#include <filesystem>
#include <memory>

class AudioPlay;
class AudioDecoder;
class AudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit AudioPlayer(QObject *parent = nullptr);
  ~AudioPlayer();
  void open(const std::filesystem::path &in_fpath);
  void play();
  void stop();
  bool is_playing();

protected slots:
  void on_request_more_data(qint64 size);

private:
  std::unique_ptr<AudioPlay> m_audio_play;
  std::unique_ptr<AudioDecoder> m_audio_decoder;
};