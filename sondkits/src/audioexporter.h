#pragma once
#include "audioinfo.h"
#include "defexports.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

struct ExportItem {
  int index;
  std::filesystem::path dest;
};

using ProgressCallback = std::function<void(float)>;

class AudioEffectsFilter;
class AudioDecoder;
class DecodeQueue;
class DataSource;
class ComposeDataSource;
class AudioEncoder;
class SOUNDKITS_API AudioExporter {
public:
  explicit AudioExporter();
  ~AudioExporter();

  static AudioFileInfo fetchAudioInfo(const std::filesystem::path &fpath,
                                      int fetch_samples_num, bool fetch_bpm,
                                      bool fetch_key);

  void open(const std::vector<std::filesystem::path> &in_fpaths);
  void stop();
  void setVolume(int stream_index, float volume);
  void setVolumeBalance(int stream_index, float balance);
  void setTempo(float tempo);
  void setSemitone(int semitone);
  void setProgressCallback(ProgressCallback progress_callback);
  bool exportFiles(const std::vector<ExportItem> &export_items);

private:
  std::shared_ptr<AudioEffectsFilter> m_com_effects_filter;
  std::vector<std::shared_ptr<AudioEffectsFilter>> m_streams_effects_filters;

  std::vector<std::shared_ptr<AudioDecoder>> m_decoders;
  std::vector<std::shared_ptr<DecodeQueue>> m_decode_queues;

  std::shared_ptr<ComposeDataSource> m_compose_source;
  std::vector<std::filesystem::path> m_in_fpaths;
  std::atomic<bool> m_stoped;
  int64_t m_max_duration_ms;
  ProgressCallback m_progress_callback;
};
