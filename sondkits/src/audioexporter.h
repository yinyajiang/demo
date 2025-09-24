#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include <atomic>

#ifdef _WIN32
#ifdef SOUNDKITS_EXPORTS
#define SOUNDKITS_API __declspec(dllexport)
#else
#define SOUNDKITS_API __declspec(dllimport)
#endif
#else
#define SOUNDKITS_API
#endif


class AudioEffectsFilter;
class AudioDecoder;
class DecodeQueue;
class DataSource;
class ComposeDataSource;
class AudioEncoder;
class SOUNDKITS_API AudioExporter  {
public:
  explicit AudioExporter();
  ~AudioExporter();

  void open(const std::vector<std::filesystem::path> &in_fpaths);
  void stop();
  void setVolume(int stream_index, float volume);
  void setVolumeBalance(int stream_index, float balance);
  void setTempo(float tempo);
  void setSemitone(int semitone);
  void exportFile(const std::filesystem::path &out_fpath);
;
private:
  std::shared_ptr<AudioEffectsFilter> m_com_effects_filter;
  std::vector<std::shared_ptr<AudioEffectsFilter>> m_streams_effects_filters;

  std::vector<std::shared_ptr<AudioDecoder>> m_decoders;
  std::vector<std::shared_ptr<DecodeQueue>> m_decode_queues;
  std::shared_ptr<AudioEncoder> m_audio_encoder;

  std::shared_ptr<ComposeDataSource> m_compose_source;
  std::vector<std::filesystem::path> m_in_fpaths;
  std::atomic<bool> m_stoped;
  int64_t m_max_duration_ms;
};