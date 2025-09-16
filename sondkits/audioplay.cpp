#include "audioplay.h"
#include <QDebug>
#include <QtGlobal>
#include <cstring>

#define DEFAULT_THRESHOLD_SECONDS 10

PCMDataSource::PCMDataSource(QAudioFormat audio_format, qint64 threshold_ms,
                             QObject *parent)
    : QIODevice(parent), m_audio_format(audio_format), m_size(0), m_pos(0),
      m_threshold_ms(threshold_ms) {
  open(QIODevice::ReadOnly);
  // 10 seconds
  // int max_size = audio_format.sampleRate() * audio_format.channelCount() *
  //                audio_format.bytesPerSample() *
  //                qMax(qint64(DEFAULT_THRESHOLD_SECONDS), threshold_ms /
  //                1000);
  int max_size = 1024 * 1024 * 100;
  m_buffer.resize(max_size);
}

PCMDataSource::~PCMDataSource() { close(); }

void PCMDataSource::clear() {
  m_pos = 0;
  m_size = 0;
}

qint64 PCMDataSource::readData(char *data, qint64 maxlen) {
  if (!data || maxlen <= 0) {
    return 0;
  }

  if (m_pos >= m_size) {
    emit signal_request_more_data(maxlen);
    return 0;
  }

  qint64 len = qMin(maxlen, m_size - m_pos);
  if (len > 0 && m_pos + len <= m_buffer.size()) {
    memcpy(data, m_buffer.constData() + m_pos, len);
    m_pos += len;

    if (available_ms() <= m_threshold_ms) {
      emit signal_request_more_data(maxlen);
    }
  } else {
    len = 0;
  }

  return len;
}

qint64 PCMDataSource::writeData(const char *data, qint64 len) { return 0; }
void PCMDataSource::add_data(const uint8_t *data, int len) {
  if (len <= 0 || !data) {
    return;
  }

  // 检查缓冲区状态和边界
  if (m_buffer.isEmpty() || m_size < 0 || m_pos < 0 ||
      m_size > m_buffer.size() || m_pos > m_size) {
    // 重置缓冲区状态
    clear();
  }

  // 检查是否有足够空间
  qint64 available_space = m_buffer.size() - m_size;

  if (len > available_space) {
    // 如果空间不足，移动数据到开头腾出更多空间
    if (m_pos > 0 && m_size > m_pos) {
      qint64 remaining_data = m_size - m_pos;
      if (remaining_data > 0 && remaining_data <= m_buffer.size() &&
          m_pos + remaining_data <= m_buffer.size()) {
        memmove(m_buffer.data(), m_buffer.data() + m_pos, remaining_data);
        m_size = remaining_data;
        m_pos = 0;
        available_space = m_buffer.size() - m_size;
      } else {
        // 数据状态异常，重置
        clear();
        available_space = m_buffer.size();
      }
    }

    // 如果仍然空间不足，只写入能容纳的部分
    if (len > available_space) {
      len = available_space;
    }
  }

  if (len <= 0 || m_size + len > m_buffer.size()) {
    return;
  }

  // 安全地写入数据
  memcpy(m_buffer.data() + m_size, data, len);
  m_size += len;

  return;
}

qint64 PCMDataSource::available_ms() {
  qint64 available_size = m_size - m_pos;
  qint64 bytes_per_second = m_audio_format.sampleRate() *
                            m_audio_format.channelCount() *
                            m_audio_format.bytesPerSample();
  if (bytes_per_second <= 0) {
    return 0;
  }
  return available_size * 1000 / bytes_per_second;
}

AudioPlay::AudioPlay(QAudioFormat audio_format, QObject *parent)
    : QObject(parent), m_audio_format(audio_format), m_volume(1.0),
      m_balance(0.0) {
  m_pcm_data_source = std::make_unique<PCMDataSource>(audio_format, 5000, this);
  auto audiodevice = QMediaDevices::defaultAudioOutput();
  if (!audiodevice.isFormatSupported(audio_format)) {
    qWarning() << "Audio format not supported";
    return;
  }
  m_audio_sink = std::make_unique<QAudioSink>(audiodevice, audio_format, this);
  m_audio_sink->setBufferSize(0);
  connect(m_pcm_data_source.get(), &PCMDataSource::signal_request_more_data,
          this, &AudioPlay::signal_request_more_data);
  connect(m_audio_sink.get(), &QAudioSink::stateChanged, this,
          &AudioPlay::on_state_changed);
  m_audio_sink->setVolume(m_volume);
}

AudioPlay::~AudioPlay() {}

void AudioPlay::on_state_changed() {
  if (m_audio_sink->state() == QAudio::IdleState) {
    // emit signal_request_more_data(1000);
  } else if (m_audio_sink->state() == QAudio::StoppedState) {
    m_pcm_data_source->clear();
  }
}

void AudioPlay::play() {
  if (m_audio_sink->state() == QAudio::StoppedState) {
    // m_pcm_data_source->clear();
    m_audio_sink->start(m_pcm_data_source.get());
  } else if (m_audio_sink->state() == QAudio::SuspendedState) {
    m_audio_sink->resume();
  }
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

void AudioPlay::input_pcm_data(const uint8_t *data, int size) {
  if (!data || size <= 0 || !m_pcm_data_source) {
    return;
  }

  // 限制单次写入的最大大小，防止过大的数据块
  // const int MAX_CHUNK_SIZE = 1024 * 1024; // 1MB
  // if (size > MAX_CHUNK_SIZE) {
  //   size = MAX_CHUNK_SIZE;
  // }

  m_pcm_data_source->add_data(data, size);
}

void AudioPlay::set_volume(float volume) {
  m_volume = qBound(0.0f, volume, 1.0f); // 确保音量在合理范围内
  if (m_audio_sink) {
    m_audio_sink->setVolume(m_volume);
  }
}

float AudioPlay::volume() { return m_volume; }
