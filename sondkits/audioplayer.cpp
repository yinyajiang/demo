#include "audioplayer.h"
#include "audiodecoder.h"
#include "audioplay.h"
#include "datasource.h"
#include "audiofilter.h"
#include "audioutils.h"
#include <iostream>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr), m_audio_filter(nullptr) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::filesystem::path &in_fpath) {
  QAudioFormat audio_format;
  audio_format.setSampleRate(DEFAULT_SAMPLE_RATE);
  audio_format.setChannelCount(DEFAULT_CHANNELS);
  switch (DEFAULT_SAMPLE_AV_FORMAT) {
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
  auto audio_decoder = std::make_shared<AudioDecoder>(
      DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, DEFAULT_SAMPLE_AV_FORMAT);
  audio_decoder->open(in_fpath);

  m_audio_filter = std::make_shared<AudioFilter>(DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, DEFAULT_SAMPLE_AV_FORMAT);
  auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
  auto data_source = std::make_shared<DecodeDataSource>(m_audio_filter, decode_queue);

  data_source->open();
  m_audio_play = std::make_unique<AudioPlay>(audio_format, data_source, this);

  auto bpm = detectAudioBPM(in_fpath);
  std::cout << "### BPM: " << bpm << std::endl;
}

void AudioPlayer::play() {
  if (m_audio_play) {
    m_audio_play->play();
    //m_audio_play->saveAsPCMFile("/Volumes/extern-usb/github/demo/sondkits/decode.pcm");
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

void AudioPlayer::setTempo(float tempo) {
  m_audio_filter->setTempo(tempo);
}