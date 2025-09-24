#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
class AudioDecoder;

struct AudioFileInfo {
  float bpm;
  int key;
  std::string key_string;
  int channels;
  int sample_rate;
  int duration_seconds;
  std::string sample_format;
  int consume_time_ms;
  std::vector<float> samples_points;
};

struct FetchConfig {
  bool fetch_bpm;
  bool fetch_key;
  int  fetch_point_num;
};

class FetchAudioInfo {
public:
  FetchAudioInfo();
  ~FetchAudioInfo();
  void abort();
  AudioFileInfo fetchAudioInfo(std::filesystem::path in_fpath, FetchConfig config);

private:
  float detectBPM(std::shared_ptr<AudioDecoder> audio_decoder);
  int detectKey(std::shared_ptr<AudioDecoder> audio_decoder);

private:
  std::atomic<bool> m_stoped;
};