#include "audioplay.h"
#include "common.h"
#include <QDebug>
#include <QFile>
#include <QtGlobal>
#include <fstream>
#include <memory>

PCMDataSourceDevice::PCMDataSourceDevice(
    std::shared_ptr<DataSource> data_source, QObject *parent)
    : QIODevice(parent), m_data_source(data_source),
      m_iodevice_played_bytes(0) {
  open(QIODevice::ReadOnly);
}

qint64 PCMDataSourceDevice::readData(char *data, qint64 size) {
#if PRINT_CONSUME_TIME
  auto start = std::chrono::high_resolution_clock::now();
#endif
  auto ret = m_data_source->readData(reinterpret_cast<uint8_t *>(data), size);
#if PRINT_CONSUME_TIME
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  qDebug() << "### readData duration: " << duration.count() << "us";
#endif

  if (ret > 0) {
    m_iodevice_played_bytes.fetch_add(ret);
  }

  return ret;
}
bool PCMDataSourceDevice::atEnd() const { return m_data_source->isEnd(); }
qint64 PCMDataSourceDevice::bytesAvailable() const {
  return m_data_source->bytesAvailable();
}

qint64 PCMDataSourceDevice::writeData(const char *, qint64) { return -1; }

void PCMDataSourceDevice::setIODevicePlayedBytes(qint64 bytes) {
  m_iodevice_played_bytes.store(bytes);
}

qint64 PCMDataSourceDevice::getIOdevicePlayedBytes() const {
  return m_iodevice_played_bytes.load();
}

int AudioPlay::getPrefferedSampleRate() {
  auto audiodevice = QMediaDevices::defaultAudioOutput();
  int sample_rate = 0;
  if(audiodevice.minimumSampleRate() == audiodevice.maximumSampleRate()) {
    sample_rate = audiodevice.minimumSampleRate();
  }else if(audiodevice.minimumSampleRate() <= 44100){
    sample_rate = 44100;
  }
  if(sample_rate == 0) {
    sample_rate = audiodevice.maximumSampleRate();
  }
  return sample_rate;
}

AudioPlay::AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<DataSource> source, QObject *parent)
    : QObject(parent), m_audio_format(audio_format), m_tempo(1.0){

  auto audiodevice = QMediaDevices::defaultAudioOutput();
  if (!audiodevice.isFormatSupported(audio_format)) {
    qWarning() << "******* Audio format not supported ***** ";
    qWarning() << "******* sample rate: " << audio_format.sampleRate() << " ***** ";
    qWarning() << "******* channel count: " << audio_format.channelCount() << " ***** ";
    qWarning() << "******* sample format: " << audio_format.sampleFormat() << " ***** ";
  }
  m_audio_sink = std::make_unique<QAudioSink>(audiodevice, audio_format, this);
  m_pcm_source = std::make_shared<PCMDataSourceDevice>(source, this);

  // 100ms buffer
  auto sinkBuff =
      audio_format.bytesPerFrame() * audio_format.sampleRate() * 100 / 1000;
  m_audio_sink->setBufferSize(sinkBuff * MAX_TEMPO);
  m_audio_sink->setVolume(1.0);

  connect(m_audio_sink.get(), &QAudioSink::stateChanged, this,
          &AudioPlay::onStateChanged);
}

AudioPlay::~AudioPlay() {
  if (m_audio_sink) {
    // 断开信号连接，防止析构过程中触发槽函数
    disconnect(m_audio_sink.get(), &QAudioSink::stateChanged, this,
               &AudioPlay::onStateChanged);
    stop();
  }
}

void AudioPlay::onStateChanged() {
  if (m_audio_sink) {
    emit signalStateChanged(m_audio_sink->state());
  }
}

void AudioPlay::play() {
  if (!m_pcm_source || !m_audio_sink) {
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
  if (m_audio_sink && m_audio_sink->state() == QAudio::ActiveState) {
    m_audio_sink->stop();
  }
}

void AudioPlay::pause() {
  if (m_audio_sink && m_audio_sink->state() == QAudio::ActiveState) {
    m_audio_sink->suspend();
  }
}

bool AudioPlay::isPlaying() {
  if (!m_audio_sink) {
    return false;
  }
  return m_audio_sink->state() == QAudio::ActiveState ||
         m_audio_sink->state() == QAudio::IdleState;
}

void AudioPlay::setSinkVoulme(float volume) {
  auto sink_volume = qBound(0.0f, volume, 1.0f); // 确保音量在合理范围内
  if (m_audio_sink) {
    m_audio_sink->setVolume(sink_volume);
  }
}

float AudioPlay::sinkVolume() { return m_audio_sink->volume(); }

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

void AudioPlay::setPlayedPositionMs(int64_t position_ms) {
  auto bytes_per_second =
      m_audio_format.bytesPerFrame() * m_audio_format.sampleRate();

  int64_t original_time_ms = static_cast<int64_t>(position_ms / m_tempo.load());
  int64_t target_bytes = static_cast<int64_t>(original_time_ms * bytes_per_second / 1000);
  
  if (m_pcm_source) {
    m_pcm_source->setIODevicePlayedBytes(target_bytes);
  }
}

void AudioPlay::setTempo(float tempo) {
  int64_t cur_position_ms = getPlayedPositionMs();
  m_tempo.store(tempo);
  setPlayedPositionMs(cur_position_ms);
}

int64_t AudioPlay::getPlayedPositionMs() const {
  int64_t bytes_per_second =
      m_audio_format.bytesPerFrame() * m_audio_format.sampleRate();
  int64_t position_ms = 0;
  if (bytes_per_second > 0 && m_pcm_source) {
    int64_t played_bytes = m_pcm_source->getIOdevicePlayedBytes();
    int64_t original_time_ms = played_bytes * 1000 / bytes_per_second;
    position_ms = static_cast<int64_t>(original_time_ms * m_tempo.load());
  }
  return position_ms;
}
