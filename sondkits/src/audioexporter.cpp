#include "audioexporter.h"
#include "audiodecoder.h"
#include "audioeffectsfilter.h"
#include "audioutils.h"
#include "composedatasource.h"
#include "decodedatasource.h"
#include "audioencoder.h"

AudioExporter::AudioExporter()
    :  m_com_effects_filter(nullptr),
      m_stoped(false) {}

AudioExporter::~AudioExporter() {}

void AudioExporter::open(const std::vector<std::filesystem::path> &in_fpaths_) {
  m_in_fpaths = in_fpaths_;
  m_stoped.store(false);

  // decoder
  m_max_duration_ms = 0;
  for (const auto &in_fpath : m_in_fpaths) {
    auto audio_decoder = std::make_shared<AudioDecoder>(
        WORKING_SAMPLE_RATE, WORKING_CHANNELS, WORKING_SAMPLE_AV_FORMAT);
    audio_decoder->open(in_fpath);
    m_decoders.push_back(audio_decoder);
    m_max_duration_ms =
        std::max(m_max_duration_ms, audio_decoder->durationSecond() * 1000);
  }

  int64_t frame_size = WORKING_CHANNELS * av_get_bytes_per_sample(WORKING_SAMPLE_AV_FORMAT);

  
  // compose filter
  AudioEffectsFilterConfig filter_config;
  filter_config.sample_rate = WORKING_SAMPLE_RATE;
  filter_config.channels = WORKING_CHANNELS;
  filter_config.format = WORKING_SAMPLE_AV_FORMAT;
  filter_config.max_tempo = MAX_TEMPO;
  m_com_effects_filter = std::make_shared<AudioEffectsFilter>(filter_config);

  // compose source
  m_compose_source = std::make_shared<ComposeDataSource>(frame_size, WORKING_SAMPLE_AV_FORMAT);
  m_compose_source->addFilter(m_com_effects_filter);

  // decode queue
  for (const auto &audio_decoder : m_decoders) {
    auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
    m_decode_queues.push_back(decode_queue);
    decode_queue->start();

    auto source = std::make_shared<DecodeDataSource>(
        frame_size, decode_queue);

    // stream filter
    auto stream_effects_filter = std::make_shared<AudioEffectsFilter>(filter_config);
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
  if (m_audio_encoder) {
    m_audio_encoder->close();
  }
}


void AudioExporter::setVolume(int stream_index, float volume) {
  if(stream_index >= m_streams_effects_filters.size()) {
    return;
  }
  if (stream_index < 0) {
    m_com_effects_filter->setVolume(volume, -1);
  } else {
    m_streams_effects_filters[stream_index]->setVolume(volume, -1);
  }
}

void AudioExporter::setVolumeBalance(int stream_index, float balance) {
  if(stream_index >= m_streams_effects_filters.size()) {
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


void AudioExporter::exportFile(const std::filesystem::path &out_fpath) {
  std::shared_ptr<AudioEncoder> audio_encoder = std::make_shared<AudioEncoder>();
  m_audio_encoder = audio_encoder;
  m_audio_encoder->open(out_fpath, AudioEncoderConfig());

  std::vector<uint8_t> buffer(m_compose_source->frameSize() * 1024);
  while (!m_stoped.load() && !m_compose_source->isEnd()) {
    auto r = m_compose_source->readData(&buffer[0], buffer.size());
    if (r == 0) {
      continue;
    }
    m_audio_encoder->encodeData(&buffer[0], r);
  }
  if (m_stoped.load()) {
    return;
  }
  m_audio_encoder->flush();
}

