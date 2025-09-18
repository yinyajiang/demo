#include "audioplay.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <fstream>
#include <memory>

PCMDataSourceDevice::PCMDataSourceDevice(
    std::shared_ptr<DataSource> data_source, QObject *parent)
    : QIODevice(parent), m_data_source(data_source) {
  open(QIODevice::ReadOnly);
}

qint64 PCMDataSourceDevice::readData(char *data, qint64 size) {
#if PRINT_READ_CONSUME_TIME
  // 获取调用时间
  auto start = std::chrono::high_resolution_clock::now();
  auto ret = m_data_source->readData(reinterpret_cast<uint8_t *>(data), size);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  qDebug() << "### readData duration: " << duration.count() << "us";
  return ret;
#else
  return m_data_source->readData(reinterpret_cast<uint8_t *>(data), size);
#endif
}
bool PCMDataSourceDevice::atEnd() const { return m_data_source->isEnd(); }
qint64 PCMDataSourceDevice::bytesAvailable() const {
  return m_data_source->bytesAvailable();
}

qint64 PCMDataSourceDevice::writeData(const char *, qint64) { return -1; }

AudioPlay::AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<DataSource> source, QObject *parent)
    : QObject(parent), m_audio_format(audio_format), m_volume(1.0),
      m_balance(0.0) {

  auto audiodevice = QMediaDevices::defaultAudioOutput();
  if (!audiodevice.isFormatSupported(audio_format)) {
    qWarning() << "Audio format not supported";
    return;
  }
  m_audio_sink = std::make_unique<QAudioSink>(audiodevice, audio_format, this);
  m_pcm_source = std::make_shared<PCMDataSourceDevice>(source, this);

  // 100ms buffer
  auto sinkBuff =
      audio_format.bytesPerFrame() * audio_format.sampleRate() * 100 / 1000;
  m_audio_sink->setBufferSize(sinkBuff * MAX_TEMPO);
  m_audio_sink->setVolume(m_volume);
}

AudioPlay::~AudioPlay() {}

void AudioPlay::on_state_changed() {}

void AudioPlay::play() {
  if (!m_pcm_source) {
    return;
  }
  m_audio_sink->start(m_pcm_source.get());
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

bool AudioPlay::isPlaying() {
  return m_audio_sink->state() == QAudio::ActiveState ||
         m_audio_sink->state() == QAudio::IdleState;
}

void AudioPlay::setSinkVoulme(float volume) {
  m_volume = qBound(0.0f, volume, 1.0f); // 确保音量在合理范围内
  if (m_audio_sink) {
    m_audio_sink->setVolume(m_volume);
  }
}

float AudioPlay::sinkVolume() { return m_volume; }

void AudioPlay::saveAsPCMFile(const std::filesystem::path &file_path) {
  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    qWarning() << "Failed to open file: " << file_path.string();
    return;
  }
  char buffer[1024 * 1024];
  while (!m_pcm_source->atEnd()) {
    auto data = m_pcm_source->readData(buffer, sizeof(buffer));
    file.write(buffer, data);
  }
  file.close();
  qDebug() << "### Saved PCM file: " << file_path.string();
}
