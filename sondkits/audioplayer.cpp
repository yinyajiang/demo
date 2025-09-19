#include "audioplayer.h"
#include "audiodecoder.h"
#include "audiofilter.h"
#include "audioplay.h"
#include "audioutils.h"
#include "datasource.h"
#include <iostream>
#include "BPMDetect.h"
#include <chrono>
extern "C" {
  #include "aubio.h"
}

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

float AudioPlayer::detectBPMUseSoundtouch() {
  if (!m_audio_decoder) {
    return 0;
  }
  int channels = 1;
  auto new_audio_decoder = std::make_shared<AudioDecoder>(
      m_audio_decoder->sampleRate(), channels, AV_SAMPLE_FMT_FLT);
  new_audio_decoder->open(m_in_fpath);

  soundtouch::BPMDetect bpm(channels, m_audio_decoder->sampleRate());

  foreachDecoderData(new_audio_decoder, [&](uint8_t *data, int size) {
    auto num_samples = size / sizeof(soundtouch::SAMPLETYPE) / channels;
    bpm.inputSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data),
                    num_samples);
    return !m_stoped.load();
  });
  new_audio_decoder->close();
  if (m_stoped.load()) {
    return 0;
  }
  auto r = bpm.getBpm();
  return r + 0.5f;
}

float AudioPlayer::detectBPMUseAubio() {
  if (!m_audio_decoder) {
    return 0;
  }
  int channels = 1;
  auto new_audio_decoder = std::make_shared<AudioDecoder>(
      m_audio_decoder->sampleRate(), channels, AV_SAMPLE_FMT_FLT);
  new_audio_decoder->open(m_in_fpath);

  int hop_size = 128;
  int buf_size = 1024; 
  auto tempo =
      new_aubio_tempo("default", buf_size, hop_size, new_audio_decoder->targetSampleRate());

  if (!tempo) {
      return 0;
  }

  fvec_t *input_vec = new_fvec(hop_size);
  fvec_t *output_vec = new_fvec(2);

  // 检查向量是否创建成功
  if (!input_vec || !output_vec) {
      if (input_vec) del_fvec(input_vec);
      if (output_vec) del_fvec(output_vec);
      del_aubio_tempo(tempo);
      return 0;
  }

  int hop_byte_size = hop_size*sizeof(float);
  int batch_size = 1024;
  int batch_byte_size = hop_byte_size*batch_size;

  foreachDecoderData(
      new_audio_decoder,
      [&](uint8_t *data, int size) {
        if (!data || size <= 0) {
          return true;
        }

        for (int i = 0; i < size / hop_byte_size; i++) {
          fvec_zeros(input_vec);
          memcpy(input_vec->data, data + i * hop_byte_size, hop_byte_size);
          aubio_tempo_do(tempo, input_vec, output_vec);
        }

        if (size % hop_byte_size != 0) {
          fvec_zeros(input_vec);
          int offset = size / hop_byte_size * hop_byte_size;
          memcpy(input_vec->data, data + offset, size - offset);
          aubio_tempo_do(tempo, input_vec, output_vec);
        }

        return !m_stoped.load();
      },
      batch_byte_size, batch_byte_size);

    float bpm = 0;
    if (!m_stoped.load()) {
      bpm = aubio_tempo_get_bpm(tempo);
    } 
    del_aubio_tempo(tempo);
    del_fvec(input_vec);
    del_fvec(output_vec);
    new_audio_decoder->close();
    return bpm;
}


AudioInfo AudioPlayer::fetchAudioInfo() {
  AudioInfo info;
  info.key = 0;
auto start_time = std::chrono::high_resolution_clock::now();
#if USE_AUBIO_BPM
  info.bpm = detectBPMUseAubio();
#else
  info.bpm = detectBPMUseSoundtouch();
#endif
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
#if PRINT_READ_CONSUME_TIME
  std::cout << "### detct bpm duration: " << duration.count()
            << "ms";
#endif
  
  info.channels = m_audio_decoder->channels();
  info.sample_rate = m_audio_decoder->sampleRate();
  info.duration_seconds = (int)m_audio_decoder->duration();
  info.sample_format = av_get_sample_fmt_name(m_audio_decoder->sampleFormat());
  info.consume_time_ms = duration.count();
  return info;
}
