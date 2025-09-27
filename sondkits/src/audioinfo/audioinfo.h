#pragma once

#include "defexports.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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
  std::filesystem::path thumbnail;
  int64_t convert_to_wav_size;
  int64_t convert_to_mp3_size;
};

struct FetchConfig {
  bool fetch_bpm;
  bool fetch_key;
  int fetch_point_num;
  bool fetch_thumbnail;
  std::filesystem::path cache_dir;
};

class AudioDecoder;
class SONDKITS_API FetchAudioInfo {
public:
  FetchAudioInfo();
  ~FetchAudioInfo();
  void abort();
  AudioFileInfo fetchAudioInfo(std::filesystem::path in_fpath,
                               FetchConfig config);

private:
  float detectBPM(std::shared_ptr<AudioDecoder> audio_decoder);
  int detectKey(std::shared_ptr<AudioDecoder> audio_decoder);
  int64_t
  calculateConvertToWavSize(std::shared_ptr<AudioDecoder> audio_decoder);
  int64_t
  calculateConvertToMp3Size(std::shared_ptr<AudioDecoder> audio_decoder);
  std::filesystem::path
  fetchThumbnail(std::shared_ptr<AudioDecoder> audio_decoder,
                 std::filesystem::path thumbnail_dir);

  bool fetchThumbnailFromMetadata(std::shared_ptr<AudioDecoder> audio_decoder,
                                  std::filesystem::path thumbnail);
  bool fetchThumbnailFromAddStream(std::shared_ptr<AudioDecoder> audio_decoder,
                                   std::filesystem::path thumbnail);

private:
  std::atomic<bool> m_stoped;
};