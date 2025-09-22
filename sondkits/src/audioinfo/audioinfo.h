#pragma once

#include <filesystem>
#include <memory>
#include <string>
class AudioDecoder;

struct AudioInfo {
  float bpm;
  int key;
  std::string key_string;
  int channels;
  int sample_rate;
  int duration_seconds;
  std::string sample_format;
  int consume_time_ms;
};

class FetchAudioInfo {
public:
  FetchAudioInfo();
  ~FetchAudioInfo();
  void stop();
  AudioInfo fetchAudioInfo(std::filesystem::path in_fpath);

private:
  float detectBPMUseSoundtouch(std::shared_ptr<AudioDecoder> audio_decoder);
  float detectBPMUseAubio(std::shared_ptr<AudioDecoder> audio_decoder);
  float demo(std::shared_ptr<AudioDecoder> audio_decoder);
  int demo2(std::shared_ptr<AudioDecoder> audio_decoder);

private:
  std::atomic<bool> m_stoped;
};