#include "audioplay.h"
#include <QDebug>
#include <QtGlobal>
#include <cstring>
#include <QFile>
#include "audiodecoder.h"
#include <fstream>
#include <iostream>

#define DEFAULT_THRESHOLD_SECONDS 10


AudioPlay::AudioPlay(QAudioFormat audio_format,
                     std::shared_ptr<DecodeQueue> decode_queue, QObject *parent)
    : QObject(parent), m_audio_format(audio_format), m_decode_queue(decode_queue), m_volume(1.0),
      m_balance(0.0) {
  
  m_pcm_data_source = std::make_unique<PCMDataSource>(decode_queue, this);
  auto audiodevice = QMediaDevices::defaultAudioOutput();
  if (!audiodevice.isFormatSupported(audio_format)) {
    qWarning() << "Audio format not supported";
    return;
  }
  m_audio_sink = std::make_unique<QAudioSink>(audiodevice, audio_format, this);
  //m_audio_sink->setBufferSize(0);
  m_audio_sink->setVolume(m_volume);
}

AudioPlay::~AudioPlay() {}

void AudioPlay::on_state_changed() {

}

void AudioPlay::play() {
  if (!m_pcm_data_source) {
    return;
  }
  m_decode_queue->start();
  while (!m_decode_queue->is_decode_stopped()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  //m_audio_sink->start(new PCMDataSourceFile("/Volumes/extern-usb/github/demo/sondkits/decode.pcm", this));
  m_audio_sink->start(m_pcm_data_source.get());

  // auto size =m_decode_queue->bytes_available();
  // char* data = new char[size];
  // auto readed =m_decode_queue->read_data(reinterpret_cast<uint8_t *>(data), size);
  // m_audio_sink->start(new PCMDataSourceMemory(data, size, this));

  
  //保存音频文件
  // std::ofstream file("/Volumes/extern-usb/github/demo/sondkits/decode.pcm", std::ios::binary);
  // while (1) {
  //   auto size = m_decode_queue->bytes_available();
  //   if (size <= 0) {
  //     break;
  //   }
  //   char data[1024*1024];
  //   auto readed =
  //       m_decode_queue->read_data(reinterpret_cast<uint8_t *>(data), 1024*1024);
  //   if (readed > 0) {
  //     file.write(reinterpret_cast<char *>(data), readed);
  //   }
  // }
  // file.close();

  // QFile *qfile = new QFile("/Volumes/extern-usb/github/demo/sondkits/decode.pcm");
  // qfile->open(QIODevice::ReadOnly);
  // m_audio_sink->start(qfile);


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
