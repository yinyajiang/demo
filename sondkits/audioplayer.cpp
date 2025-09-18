#include "audioplayer.h"
#include "audiodecoder.h"
#include "audiofilter.h"
#include "audioplay.h"
#include "audioutils.h"
#include "datasource.h"
#include <iostream>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr), m_audio_filter(nullptr) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::filesystem::path &in_fpath) {
  // decoder
  auto audio_decoder = std::make_shared<AudioDecoder>(
      DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, DEFAULT_SAMPLE_AV_FORMAT);
  audio_decoder->open(in_fpath);

  // audio play
  QAudioFormat audio_format;
  audio_format.setSampleRate(audio_decoder->targetSampleRate());
  audio_format.setChannelCount(audio_decoder->targetChannels());
  switch (audio_decoder->targetSampleFormat()) {
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
  AudioFilterConfig filter_config;
  filter_config.sample_rate = audio_decoder->targetSampleRate();
  filter_config.channels = audio_decoder->targetChannels();
  filter_config.format = audio_decoder->targetSampleFormat();
  filter_config.max_tempo = MAX_TEMPO;
  m_audio_filter = std::make_shared<AudioFilter>(filter_config);

  // decode queue
  auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);

  // data source
  auto data_source = std::make_shared<DecodeDataSource>(
      m_audio_filter, audio_format.bytesPerFrame(), decode_queue);
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
  if (m_audio_play) {
    m_audio_play->stop();
  }
}

bool AudioPlayer::isPlaying() {
  if (m_audio_play) {
    return m_audio_play->isPlaying();
  }
  return false;
}

void AudioPlayer::setVolume(float volume) {
  m_audio_filter->setVolume(volume, -1);
}

void AudioPlayer::setVolumeBalance(float balance) {
  m_audio_filter->setVolumeBalance(balance);
}

void AudioPlayer::setTempo(float tempo) { m_audio_filter->setTempo(tempo); }

float AudioPlayer::detectBPM(const std::filesystem::path &in_fpath) {
  return detectAudioBPM(in_fpath);
}