#include "audiofilter.h"
extern "C" {
#include <libavutil/avutil.h>
}
#include "SoundTouch.h"
#include <cassert>
#include <cstdint>
#include <iostream>

AudioFilter::AudioFilter(AudioFilterConfig config) : m_config(config) {
  assert(!av_sample_fmt_is_planar(config.format));
  m_sample_size = av_get_bytes_per_sample(config.format);
  m_volume.store(1.0f);
  for (int i = 0; i < m_config.channels; i++) {
    m_channels_volumes[i].store(1.0f);
  }

  m_tempo = 1.0f;
  newSoundTouch();
}

AudioFilter::~AudioFilter() {}

float AudioFilter::volume(int channel_num) {
  if (channel_num == -1) {
    return m_volume.load();
  }
  return m_channels_volumes[channel_num].load();
}

void AudioFilter::setVolume(float volume, int channel_num) {
  if (volume > 1.0f)
    volume = 1.0f;
  else if (volume < 0.0f)
    volume = 0.0f;
  if (channel_num == -1) {
    m_volume.store(volume);
  } else {
    m_channels_volumes[channel_num].store(volume);
  }
}

void AudioFilter::setTempo(float tempo) {
  if (tempo <= 0.1f)
    return;
  if (tempo > m_config.max_tempo)
    tempo = m_config.max_tempo;
  m_tempo = tempo;
  newSoundTouch();
}

void AudioFilter::setVolumeBalance(float balance) {
  if (m_config.channels != 2 || balance > 1.0f || balance < -1.0f) {
    return;
  }
  if (balance > 0.0f) { // 偏向右声道, 减小左声道音量
    setVolume(1.0f - balance, 0);
  } else { // 偏向左声道, 减小右声道音量
    setVolume(1.0f + balance, 1);
  }
}

void AudioFilter::process(uint8_t *data, int64_t *size) {
  applyTempo(data, size);
  applyVolume(data, size);
}

int64_t AudioFilter::flushRemaining() {
  int64_t num_samples = 0;
  m_sound_touch_lock.lock();
  if (m_soundtouch) {
    if (!m_soundtouch_flushed) {
      m_soundtouch->flush();
      m_soundtouch_flushed = true;
    }
    num_samples = m_soundtouch->numSamples();
  }
  m_sound_touch_lock.unlock();
  return num_samples * m_config.channels * m_sample_size;
}

void AudioFilter::reciveRemaining(uint8_t *data, int64_t *size) {
  m_sound_touch_lock.lock();
  if (m_soundtouch) {
    auto num_samples =
        *size / sizeof(soundtouch::SAMPLETYPE) / m_config.channels;
    auto num = m_soundtouch->receiveSamples(
        reinterpret_cast<soundtouch::SAMPLETYPE *>(data), num_samples);
    *size = num * sizeof(soundtouch::SAMPLETYPE) * m_config.channels;
  }
  m_sound_touch_lock.unlock();
}

void AudioFilter::applyVolume(uint8_t *data, int64_t *size) {
  if (!data || !size || *size <= 0) {
    return;
  }
  const int samples = *size / m_sample_size; // 交错采样，逐样本缩放即可
  switch (m_config.format) {
  case AV_SAMPLE_FMT_U8: {
    forEachChannelSample<uint8_t>(
        data, *size, [this](uint8_t *data, int channel_num) {
          applyU8SampleVolume(data, m_channels_volumes[channel_num].load() *
                                        m_volume.load());
        });
    break;
  }
  case AV_SAMPLE_FMT_S16: {
    forEachChannelSample<int16_t>(
        data, *size, [this](int16_t *data, int channel_num) {
          applySignedSampleVolume<int16_t, float>(
              data, m_channels_volumes[channel_num].load() * m_volume.load());
        });
    break;
  }
  case AV_SAMPLE_FMT_S32: {
    forEachChannelSample<int32_t>(
        data, *size, [this](int32_t *data, int channel_num) {
          applySignedSampleVolume<int32_t, double>(
              data, m_channels_volumes[channel_num].load() * m_volume.load());
        });
    break;
  }
  case AV_SAMPLE_FMT_S64: {
    forEachChannelSample<int64_t>(
        data, *size, [this](int64_t *data, int channel_num) {
          applySignedSampleVolume<int64_t, long double>(
              data, m_channels_volumes[channel_num].load() * m_volume.load());
        });
    break;
  }
  case AV_SAMPLE_FMT_FLT: {
    forEachChannelSample<float>(
        data, *size, [this](float *data, int channel_num) {
          applySignedSampleVolume<float, float>(
              data, m_channels_volumes[channel_num].load() * m_volume.load());
        });
    break;
  }
  case AV_SAMPLE_FMT_DBL: {
    forEachChannelSample<double>(
        data, *size, [this](double *data, int channel_num) {
          applySignedSampleVolume<double, double>(
              data, m_channels_volumes[channel_num].load() * m_volume.load());
        });
    break;
  }
  default:
    break;
  }
}

void AudioFilter::applyTempo(uint8_t *data, int64_t *size) {
  if (!data || !size || *size <= 0 || m_config.format != AV_SAMPLE_FMT_FLT) {
    return;
  }
  m_sound_touch_lock.lock();
  std::cout << "### applyTempo lock " << std::endl;
  if (m_tempo != 1.0f && m_soundtouch) {
    auto num_samples =
        *size / sizeof(soundtouch::SAMPLETYPE) / m_config.channels;
    m_soundtouch->putSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data),
                             num_samples);
    auto num = m_soundtouch->receiveSamples(
        reinterpret_cast<soundtouch::SAMPLETYPE *>(data), num_samples);
    if (num == 0) {
       *size = -1;
    }else{
       *size = num * sizeof(soundtouch::SAMPLETYPE) * m_config.channels;
    }
    std::cout << "### applyTempo num: " << num << std::endl;
  }
  m_sound_touch_lock.unlock();
  std::cout << "### applyTempo unlock " << std::endl;
}

void AudioFilter::applyU8SampleVolume(uint8_t *data, float volume) {
  int v = static_cast<int>(*data) - 128; // 转为有符号中心 0
  float scaled = v * volume;
  if (scaled > 127.0f)
    scaled = 127.0f;
  else if (scaled < -128.0f)
    scaled = -128.0f;
  int out = static_cast<int>(scaled) + 128;
  if (out < 0) {
    out = 0;
  } else if (out > 255) {
    out = 255;
  }
  *data = static_cast<uint8_t>(out);
}

void AudioFilter::newSoundTouch() {
  m_sound_touch_lock.lock();
  if (m_soundtouch) {
    m_soundtouch->clear();
    m_soundtouch_flushed = false;
  }
  m_soundtouch = std::make_unique<soundtouch::SoundTouch>();
  m_soundtouch->setSampleRate(m_config.sample_rate);
  m_soundtouch->setChannels(m_config.channels);
  m_soundtouch->setTempo(m_tempo);
  m_sound_touch_lock.unlock();
}
