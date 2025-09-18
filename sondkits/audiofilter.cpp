#include "audiofilter.h"
extern "C" {
#include <libavutil/avutil.h>
}
#include <cassert>
#include <cstdint>
#include "SoundTouch.h"

AudioFilter::AudioFilter(int sample_rate, int channels, AVSampleFormat format, float max_tempo)
    : m_sample_rate(sample_rate), m_channels(channels), m_format(format), m_max_tempo(max_tempo) {
  assert(!av_sample_fmt_is_planar(format));
  m_sample_size = av_get_bytes_per_sample(format);
  m_volume.store(1.0f);
  for (int i = 0; i < m_channels; i++) {
    m_channels_volumes[i].store(1.0f);
  }
  
  m_sound_touch = std::make_unique<soundtouch::SoundTouch>();
  m_sound_touch->setSampleRate(sample_rate);
  m_sound_touch->setChannels(channels);
  m_sound_touch->setTempo(1.0f);
  m_tempo = 1.0f;
}

AudioFilter::~AudioFilter() {}

float AudioFilter::volume(int channel_num) {
  if (channel_num == -1) {
    return m_volume.load();
  }
  return m_channels_volumes[channel_num].load();
}

void AudioFilter::setVolume(float volume, int channel_num) {
  if (volume > 1.0f) volume = 1.0f;
  else if (volume < 0.0f) volume = 0.0f;
  if (channel_num == -1) {
    m_volume.store(volume);
  } else {
    m_channels_volumes[channel_num].store(volume);
  }
}

void AudioFilter::setTempo(float tempo) {
  if (tempo <= 0.1f) return;
  if (tempo > m_max_tempo)  tempo = m_max_tempo;
  m_sound_touch_lock.lock();
  m_sound_touch = std::make_unique<soundtouch::SoundTouch>();
  m_sound_touch->setSampleRate(m_sample_rate);
  m_sound_touch->setChannels(m_channels);
  m_sound_touch->setTempo(tempo);
  m_tempo = tempo;
  m_sound_touch_lock.unlock();
}

void AudioFilter::setVolumeBalance(float balance) {
  if(m_channels != 2 || balance > 1.0f || balance < -1.0f) {
    return;
  }
  if (balance > 0.0f) { // 偏向右声道, 减小左声道音量
    setVolume(1.0f-balance, 0);
  } else { // 偏向左声道, 减小右声道音量
    setVolume(1.0f+balance, 1);
  }
}

void AudioFilter::process(uint8_t *data, int64_t *size) {
  applyTempo(data, size);
  applyVolume(data, size);
}

void AudioFilter::applyVolume(uint8_t *data, int64_t *size) {
  if (!data || !size || *size <= 0) {
    return;
  }
  const int samples = *size / m_sample_size; // 交错采样，逐样本缩放即可
  switch (m_format) {
    case AV_SAMPLE_FMT_U8: {
        forEachChannelSample<uint8_t>(data, *size, [this](uint8_t *data, int channel_num) {
            applyU8SampleVolume(data, m_channels_volumes[channel_num].load()*m_volume.load());
        });
        break;
    }
    case AV_SAMPLE_FMT_S16: {
        forEachChannelSample<int16_t>(data, *size, [this](int16_t *data, int channel_num) {
            applySignedSampleVolume<int16_t, float>(data, m_channels_volumes[channel_num].load()*m_volume.load());
        });
        break;
    }
    case AV_SAMPLE_FMT_S32: {
        forEachChannelSample<int32_t>(data, *size, [this](int32_t *data, int channel_num) {
            applySignedSampleVolume<int32_t, double>(data, m_channels_volumes[channel_num].load()*m_volume.load());
        });
        break;
    }
    case AV_SAMPLE_FMT_S64: {
        forEachChannelSample<int64_t>(data, *size, [this](int64_t *data, int channel_num) {
            applySignedSampleVolume<int64_t, long double>(data, m_channels_volumes[channel_num].load()*m_volume.load());
        });
        break;
    }
  case AV_SAMPLE_FMT_FLT: {
    forEachChannelSample<float>(data, *size, [this](float *data, int channel_num) {
        applySignedSampleVolume<float, float>(data, m_channels_volumes[channel_num].load()*m_volume.load());
    });
    break;
  }
  case AV_SAMPLE_FMT_DBL: {
    forEachChannelSample<double>(data, *size,
                                 [this](double *data, int channel_num) {
        applySignedSampleVolume<double, double>(data, m_channels_volumes[channel_num].load()*m_volume.load());
    });
    break;
  }
  default:
    break;
  }
}

void AudioFilter::applyTempo(uint8_t *data, int64_t *size) {
  if (!data || !size || *size <= 0 || m_format != AV_SAMPLE_FMT_FLT) {
    return;
  }
  m_sound_touch_lock.lock();
  if (m_tempo != 1.0f) {
    auto num_samples = *size / sizeof(soundtouch::SAMPLETYPE) / m_channels;
    m_sound_touch->putSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data), num_samples);
    auto num = m_sound_touch->receiveSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data), num_samples);
    *size = num * sizeof(soundtouch::SAMPLETYPE) * m_channels;
  }
  m_sound_touch_lock.unlock();
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
    }
    else if (out > 255) {
      out = 255;
    }
    *data = static_cast<uint8_t>(out);
}
