#include "audioexporter.h"
#include "audiodecoder.h"
#include "audioeffectsfilter.h"
#include "audioencoder.h"
#include "audioinfo.h"
#include "audioplay.h"
#include "common.h"
#include "composedatasource.h"
#include "decodedatasource.h"
#include "encodefilter.h"
#include "progressfilter.h"

AudioFileInfo AudioExporter::fetchAudioInfo(const std::filesystem::path &fpath,
                                            int fetch_samples_num,
                                            bool fetch_bpm, bool fetch_key) {
  FetchConfig fetch_config;
  fetch_config.fetch_bpm = fetch_bpm;
  fetch_config.fetch_key = fetch_key;
  fetch_config.fetch_point_num = fetch_samples_num;
  FetchAudioInfo fetcher;
  return fetcher.fetchAudioInfo(fpath, fetch_config);
}

AudioExporter::AudioExporter()
    : m_com_effects_filter(nullptr), m_stoped(false) {}

AudioExporter::~AudioExporter() {}

void AudioExporter::open(const std::vector<std::filesystem::path> &in_fpaths_) {
  m_in_fpaths = in_fpaths_;
  m_stoped.store(false);
  m_progress_callback = nullptr;

  SET_WORKING_SAMPLE_RATE(AudioPlay::getPrefferedSampleRate());

  // decoder
  m_max_duration_ms = 0;
  for (const auto &in_fpath : m_in_fpaths) {
    auto audio_decoder = std::make_shared<AudioDecoder>(
        WORKING_SAMPLE_RATE(), WORKING_CHANNELS, WORKING_SAMPLE_AV_FORMAT);
    audio_decoder->open(in_fpath);
    m_decoders.push_back(audio_decoder);
    m_max_duration_ms =
        std::max(m_max_duration_ms, audio_decoder->durationSecond() * 1000);
  }

  int64_t frame_size =
      WORKING_CHANNELS * av_get_bytes_per_sample(WORKING_SAMPLE_AV_FORMAT);

  // compose filter
  AudioEffectsFilterConfig filter_config;
  filter_config.sample_rate = WORKING_SAMPLE_RATE();
  filter_config.channels = WORKING_CHANNELS;
  filter_config.format = WORKING_SAMPLE_AV_FORMAT;
  filter_config.max_tempo = MAX_TEMPO;
  m_com_effects_filter = std::make_shared<AudioEffectsFilter>(filter_config);

  // compose source
  m_compose_source =
      std::make_shared<ComposeDataSource>(frame_size, WORKING_SAMPLE_AV_FORMAT);
  m_compose_source->addFilter(m_com_effects_filter);

  // decode queue
  for (const auto &audio_decoder : m_decoders) {
    auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
    m_decode_queues.push_back(decode_queue);
    decode_queue->start();

    auto source = std::make_shared<DecodeDataSource>(frame_size, decode_queue);

    // stream filter
    auto stream_effects_filter =
        std::make_shared<AudioEffectsFilter>(filter_config);
    m_streams_effects_filters.push_back(stream_effects_filter);
    source->addFilter(stream_effects_filter);

    m_compose_source->addDataSource(source);
  }
}

void AudioExporter::stop() {
  m_stoped.store(true);
  for (const auto &audio_decoder : m_decoders) {
    audio_decoder->close();
  }
  for (const auto &decode_queue : m_decode_queues) {
    decode_queue->stop();
  }
}

void AudioExporter::setVolume(int stream_index, float volume) {
  if (stream_index >= m_streams_effects_filters.size()) {
    return;
  }
  if (stream_index < 0) {
    m_com_effects_filter->setVolume(volume, -1);
  } else {
    m_streams_effects_filters[stream_index]->setVolume(volume, -1);
  }
}

void AudioExporter::setVolumeBalance(int stream_index, float balance) {
  if (stream_index >= m_streams_effects_filters.size()) {
    return;
  }
  if (stream_index < 0) {
    m_com_effects_filter->setVolumeBalance(balance);
  } else {
    m_streams_effects_filters[stream_index]->setVolumeBalance(balance);
  }
}

void AudioExporter::setTempo(float tempo) {
  m_com_effects_filter->setTempo(tempo);
}

void AudioExporter::setSemitone(int semitone) {
  m_com_effects_filter->setSemitone(semitone);
}

void AudioExporter::setProgressCallback(ProgressCallback progress_callback) {
  m_progress_callback = progress_callback;
}

bool AudioExporter::exportFiles(const std::vector<ExportItem> &export_items) {
  std::vector<std::shared_ptr<AudioEncoder>> audio_encoders;
  AudioEncoderConfig encoder_config;
  encoder_config.in_sample_format = WORKING_SAMPLE_AV_FORMAT;
  encoder_config.in_sample_rate = WORKING_SAMPLE_RATE();
  encoder_config.in_channels = WORKING_CHANNELS;
  encoder_config.out_codec_id = AV_CODEC_ID_NONE;
  encoder_config.out_sample_rate = WORKING_SAMPLE_RATE();
  encoder_config.out_channels = WORKING_CHANNELS;
  encoder_config.out_sample_format = WORKING_SAMPLE_AV_FORMAT;

  // 配置编码器
  for (const auto &export_item : export_items) {
    if (export_item.dest.empty() ||
        export_item.index > m_compose_source->dataSourceCount()) {
      continue;
    }
    auto encoder = std::make_shared<AudioEncoder>();
    encoder->open(export_item.dest, encoder_config);
    audio_encoders.push_back(encoder);
    if (export_item.index == -1) {
      m_compose_source->addFilter(std::make_shared<EncodeFilter>(encoder));
    } else {
      m_compose_source->dataSource(export_item.index)
          .addFilter(std::make_shared<EncodeFilter>(encoder));
    }
  }
  // progress
  auto progress_filter = std::make_shared<ProgressFilter>(
      WORKING_SAMPLE_RATE(), WORKING_CHANNELS, WORKING_SAMPLE_AV_FORMAT,
      m_max_duration_ms / 1000, std::chrono::milliseconds(1000));
  m_compose_source->addFilter(progress_filter);
  progress_filter->setProgressCallback(m_progress_callback);

  auto b = m_compose_source->consumeAll();
  if (b) {
    for (const auto &encoder : audio_encoders) {
      encoder->flush();
      encoder->close();
    }
    progress_filter->progressFinished();
  }
  stop();
  return b;
}
