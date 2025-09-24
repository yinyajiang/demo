#include "audioplayer.h"
#include "audiodecoder.h"
#include "audioeffectsfilter.h"
#include "audioinfo.h"
#include "audioplay.h"
#include "audioutils.h"
#include "composedatasource.h"
#include "decodedatasource.h"
#include <QDebug>
#include <QtMultimedia/qaudio.h>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent), m_audio_play(nullptr), m_effects_filter(nullptr),
      m_stoped(false) {}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::open(const std::vector<QString> &in_fpaths_) {
  m_in_fpaths.clear();
  for (const auto &f : in_fpaths_) {
    m_in_fpaths.push_back(f.toStdWString());
  }
  m_stoped.store(false);

  // decoder
  m_max_duration_ms = 0;
  for (const auto &in_fpath : m_in_fpaths) {
    auto audio_decoder = std::make_shared<AudioDecoder>(
        WORKING_SAMPLE_RATE, WORKING_CHANNELS, WORKING_SAMPLE_AV_FORMAT);
    audio_decoder->open(in_fpath);
    m_audio_decoders.push_back(audio_decoder);
    m_max_duration_ms =
        std::max(m_max_duration_ms, audio_decoder->durationSecond() * 1000);
  }

  // audio play
  QAudioFormat audio_format;
  audio_format.setSampleRate(WORKING_SAMPLE_RATE);
  audio_format.setChannelCount(WORKING_CHANNELS);
  switch (WORKING_SAMPLE_AV_FORMAT) {
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
  filter_config.sample_rate = WORKING_SAMPLE_RATE;
  filter_config.channels = WORKING_CHANNELS;
  filter_config.format = WORKING_SAMPLE_AV_FORMAT;
  filter_config.max_tempo = MAX_TEMPO;
  m_effects_filter = std::make_shared<AudioEffectsFilter>(filter_config);

  // compose source
  m_compose_source = std::make_shared<ComposeDataSource>(
      audio_format.bytesPerFrame(), WORKING_SAMPLE_AV_FORMAT);
  m_compose_source->addFilter(m_effects_filter);

  // decode queue
  for (const auto &audio_decoder : m_audio_decoders) {
    auto decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
    m_decode_queues.push_back(decode_queue);
    decode_queue->start();

    auto source = std::make_shared<DecodeDataSource>(
        audio_format.bytesPerFrame(), decode_queue);
    m_compose_source->addDataSource(source);
  }

  // audio play
  m_audio_play =
      std::make_unique<AudioPlay>(audio_format, m_compose_source, this);

  m_update_timer.setInterval(1000);
  connect(&m_update_timer, &QTimer::timeout, this,
          &AudioPlayer::onUpdateTimerTimeout);

  connect(m_audio_play.get(), &AudioPlay::signalStateChanged,
          this, &AudioPlayer::onStateChanged);
}

void AudioPlayer::play() {
  if (m_audio_play) {
    m_audio_play->play();
    m_update_timer.start();
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
  m_update_timer.stop();
  for (const auto &audio_decoder : m_audio_decoders) {
    audio_decoder->close();
  }
  for (const auto &decode_queue : m_decode_queues) {
    decode_queue->stop();
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

AudioInfo AudioPlayer::fetchFullAudioInfo(QString fpath, int fetch_samples_num) {
  FetchAudioInfo fetch_audio_info;

  FetchConfig fetch_config;
  fetch_config.fetch_bpm = true;
  fetch_config.fetch_key = true;
  fetch_config.fetch_point_num = fetch_samples_num;
  auto info = fetch_audio_info.fetchAudioInfo(fpath.toStdWString(), fetch_config);
  AudioInfo audio_info;
  audio_info.bpm = info.bpm;
  audio_info.key = info.key;
  audio_info.key_string = QString::fromStdString(info.key_string);
  audio_info.channels = info.channels;
  audio_info.sample_rate = info.sample_rate;
  audio_info.duration_seconds = info.duration_seconds;
  audio_info.sample_format = QString::fromStdString(info.sample_format);
  audio_info.consume_time_ms = info.consume_time_ms;
  audio_info.samples = info.samples_points;
  return audio_info;
}

void AudioPlayer::seek(int64_t time_ms) {
  m_audio_play->pause();
  for (const auto &audio_decoder : m_audio_decoders) {
    audio_decoder->seek(time_ms);
  }
  if (m_compose_source) {
    m_compose_source->clear();
  }
  m_audio_play->setPlayedPositionMs(time_ms);
  m_compose_source->waitHasData();
  m_audio_play->play();
}

void AudioPlayer::onUpdateTimerTimeout() {
  auto played_position_ms = m_audio_play->getPlayedPositionMs();
  if (played_position_ms > m_max_duration_ms) {
    played_position_ms = m_max_duration_ms;
  }
  emit signalTimeProgress(played_position_ms / 1000);
}

void AudioPlayer::onStateChanged(int state_) {
  QAudio::State state = static_cast<QAudio::State>(state_);
  if ((state == QAudio::IdleState || state == QAudio::StoppedState) &&
    m_compose_source->isEnd()) {
    emit signalPlayFinished();
    m_update_timer.stop();
    qDebug() << "### play finished";
  }
}
