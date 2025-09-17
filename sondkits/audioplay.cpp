#include "audioplay.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>

AudioPlay::AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<PCMDataSource> source, QObject *parent)
    : QObject(parent), m_audio_format(audio_format), m_volume(1.0),
      m_balance(0.0), m_pcm_data_source(source) {

  auto audiodevice = QMediaDevices::defaultAudioOutput();
  if (!audiodevice.isFormatSupported(audio_format)) {
    qWarning() << "Audio format not supported";
    return;
  }
  m_audio_sink = std::make_unique<QAudioSink>(audiodevice, audio_format, this);
  // m_audio_sink->setBufferSize(0);
  m_audio_sink->setVolume(m_volume);
}

AudioPlay::~AudioPlay() {}

void AudioPlay::on_state_changed() {}

void AudioPlay::play() {
  if (!m_pcm_data_source) {
    return;
  }
  m_pcm_data_source->start();
  m_audio_sink->start(m_pcm_data_source.get());
  // if (m_audio_sink->state() == QAudio::StoppedState) {
  //   // m_pcm_data_source->clear();

  // } else if (m_audio_sink->state() == QAudio::SuspendedState) {
  //   m_audio_sink->resume();
  // }
}

void AudioPlay::stop() {
  if (m_audio_sink->state() == QAudio::ActiveState) {
    m_audio_sink->stop();
  }
}

void AudioPlay::pause() {
  if (m_audio_sink->state() == QAudio::ActiveState) {
    m_audio_sink->suspend();
  }
}

bool AudioPlay::is_playing() {
  return m_audio_sink->state() == QAudio::ActiveState ||
         m_audio_sink->state() == QAudio::IdleState;
}

void AudioPlay::set_volume(float volume) {
  m_volume = qBound(0.0f, volume, 1.0f); // 确保音量在合理范围内
  if (m_audio_sink) {
    m_audio_sink->setVolume(m_volume);
  }
}

float AudioPlay::volume() { return m_volume; }
