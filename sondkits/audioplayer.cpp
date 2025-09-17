#include "audioplayer.h"
#include "audiodecoder.h"
#include "audioplay.h"
#include "pcmdatasource.h"

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr) {}

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
  auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
  auto pcm_data_source = std::make_shared<DecodePCMSource>(decode_queue, this);

  m_audio_play =
      std::make_unique<AudioPlay>(audio_format, pcm_data_source, this);
}

void AudioPlayer::play() {
  if (m_audio_play) {
    m_audio_play->play();
  }
}

void AudioPlayer::stop() {
  if (m_audio_play) {
    m_audio_play->stop();
  }
}

bool AudioPlayer::is_playing() {
  if (m_audio_play) {
    return m_audio_play->is_playing();
  }
  return false;
}
