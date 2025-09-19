#include "audioplayer.h"
#include "audiodecoder.h"
#include "audiofilter.h"
#include "audioplay.h"
#include "audioutils.h"
#include "datasource.h"
#include <iostream>
#include "BPMDetect.h"

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr), m_audio_filter(nullptr), m_stoped(false) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::filesystem::path &in_fpath) {
  m_in_fpath = in_fpath;
  m_stoped.store(false);
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
  AudioFilterConfig filter_config;
  filter_config.sample_rate = m_audio_decoder->targetSampleRate();
  filter_config.channels = m_audio_decoder->targetChannels();
  filter_config.format = m_audio_decoder->targetSampleFormat();
  filter_config.max_tempo = MAX_TEMPO;
  m_audio_filter = std::make_shared<AudioFilter>(filter_config);

  // decode queue
  auto decode_queue = std::make_shared<DecodeQueue>(m_audio_decoder);

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
  m_stoped.store(true);
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

int AudioPlayer::detectBPM() {
  if (!m_audio_decoder) {
    return 0;
   }
   int channels = m_audio_decoder->channels();
    auto audio_decoder = std::make_shared<AudioDecoder>(
        m_audio_decoder->sampleRate(), channels, AV_SAMPLE_FMT_FLT);
    audio_decoder->open(m_in_fpath);
    soundtouch::BPMDetect bpm(channels, m_audio_decoder->sampleRate());
  
    foreachDecoderData(audio_decoder, [&](uint8_t *data, int size) {
      auto num_samples = size / sizeof(soundtouch::SAMPLETYPE) / channels;
      bpm.inputSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data),
                       num_samples);
      return !m_stoped.load();
    });
    audio_decoder->close();
    if (m_stoped.load()) {
      return 0;
    }
    auto r = bpm.getBpm();
    //round to nearest integer
    return static_cast<int>(r + 0.5f);
}

AudioInfo AudioPlayer::fetchAudioInfo() {
  AudioInfo info;
  info.key = 0;
  info.bpm = detectBPM();
  info.channels = m_audio_decoder->channels();
  info.sample_rate = m_audio_decoder->sampleRate();
  info.duration_seconds = (int)m_audio_decoder->duration();
  info.sample_format = av_get_sample_fmt_name(m_audio_decoder->sampleFormat());
  return info;
}
