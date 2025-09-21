#include "audioplayer.h"
#include "BPMDetect.h"
#include "audiodecoder.h"
#include "audioeffectsfilter.h"
#include "audioplay.h"
#include "audioutils.h"
#include "decodedatasource.h"
#include <chrono>
extern "C" {
#include "aubio.h"
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr), m_effects_filter(nullptr),
      m_stoped(false) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::filesystem::path &in_fpath) {
  m_in_fpath = in_fpath;
  m_stoped.store(false);

  m_audio_info_fetch = std::make_shared<FetchAudioInfo>();
  // decoder
  m_audio_decoder = std::make_shared<AudioDecoder>(
      DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, DEFAULT_SAMPLE_AV_FORMAT);
  m_audio_decoder->open(in_fpath);

  // audio play
  QAudioFormat audio_format;
  audio_format.setSampleRate(m_audio_decoder->targetSampleRate());
  audio_format.setChannelCount(m_audio_decoder->targetChannels());
  switch (m_audio_decoder->targetSampleFormat()) {
  case AV_SAMPLE_FMT_U8:
    audio_format.setSampleFormat(QAudioFormat::UInt8);
    break;
  case AV_SAMPLE_FMT_S16:
    audio_format.setSampleFormat(QAudioFormat::Int16);
    break;
  case AV_SAMPLE_FMT_S32:
    audio_format.setSampleFormat(QAudioFormat::Int32);
    break;
  case AV_SAMPLE_FMT_FLT:
    audio_format.setSampleFormat(QAudioFormat::Float);
    break;
  default:
    assert(false);
  }

  // filter
  AudioEffectsFilterConfig filter_config;
  filter_config.sample_rate = m_audio_decoder->targetSampleRate();
  filter_config.channels = m_audio_decoder->targetChannels();
  filter_config.format = m_audio_decoder->targetSampleFormat();
  filter_config.max_tempo = MAX_TEMPO;
  m_effects_filter = std::make_shared<AudioEffectsFilter>(filter_config);

  // decode queue
  auto decode_queue = std::make_shared<DecodeQueue>(m_audio_decoder);

  // data source
  auto data_source = std::make_shared<DecodeDataSource>(
      m_effects_filter, audio_format.bytesPerFrame(), decode_queue);
  data_source->open();

  m_audio_play = std::make_unique<AudioPlay>(audio_format, data_source, this);
}

void AudioPlayer::play() {
  if (m_audio_play) {
    m_audio_play->play();
    // m_audio_play->saveAsPCMFile("/Volumes/extern-usb/github/demo/sondkits/decode.pcm");
  }
}

void AudioPlayer::pause() {
  if (m_audio_play) {
    m_audio_play->pause();
  }
}

void AudioPlayer::stop() {
  m_stoped.store(true);
  if (m_audio_play) {
    m_audio_play->stop();
  }
  if (m_audio_info_fetch) {
    m_audio_info_fetch->stop();
  }
}

bool AudioPlayer::isPlaying() {
  if (m_audio_play) {
    return m_audio_play->isPlaying();
  }
  return false;
}

void AudioPlayer::setVolume(float volume) {
  m_effects_filter->setVolume(volume, -1);
}

void AudioPlayer::setVolumeBalance(float balance) {
  m_effects_filter->setVolumeBalance(balance);
}

void AudioPlayer::setTempo(float tempo) { m_effects_filter->setTempo(tempo); }

void AudioPlayer::setSemitone(int semitone) {
  m_effects_filter->setSemitone(semitone);
}

AudioInfo AudioPlayer::fetchAudioInfo() {
  if (m_audio_info_fetch) {
    return m_audio_info_fetch->fetchAudioInfo(m_in_fpath);
  }
  return AudioInfo();
}
