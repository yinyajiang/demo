#include "audioplayer.h"
#include "audiodecoder.h"
#include "audioplay.h"

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLE_AV_FORMAT AV_SAMPLE_FMT_S16

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::filesystem::path &in_fpath) {
  m_audio_decoder = std::make_unique<AudioDecoder>(
      DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, DEFAULT_SAMPLE_AV_FORMAT);
  m_audio_decoder->open(in_fpath);
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
  m_audio_play = std::make_unique<AudioPlay>(audio_format, this);
  connect(m_audio_play.get(), &AudioPlay::signal_request_more_data, this,
          &AudioPlayer::on_request_more_data);
}

void AudioPlayer::on_request_more_data(qint64 size) {
  if (!m_audio_decoder || !m_audio_play) {
    return;
  }

  int inputed_size = 0;
  while (inputed_size < size && !m_audio_decoder->is_end()) {
    int data_size = 0;
    auto data = m_audio_decoder->decode_next_frame_data(&data_size);
    if (data && data_size > 0) {
      m_audio_play->input_pcm_data(data, data_size);
      av_free(data);
      data = nullptr; // 防止重复释放
    } else if (data) {
      // 即使data_size为0，如果data不为空也需要释放
      av_free(data);
      data = nullptr;
    }
    // 如果解码出错或结束，跳出循环
    if (data_size <= 0) {
      break;
    }
    inputed_size += data_size;
  }
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