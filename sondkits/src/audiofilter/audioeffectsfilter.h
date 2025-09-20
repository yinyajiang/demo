#pragma once

#include "audiofilter.h"
#include "common.h"
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

struct AudioEffectsFilterConfig {
  int sample_rate;
  int channels;
  AVSampleFormat format;
  float max_tempo;
  float min_tempo;
};

namespace soundtouch {
class SoundTouch;
}

class AudioEffectsFilter : public AudioFilter {
public:
  AudioEffectsFilter(AudioEffectsFilterConfig config);
  ~AudioEffectsFilter();
  //[0.0, 1.0]
  void setVolume(float volume, int channel_num = -1);
  float volume(int channel_num = -1);
  //[-1.0, 1.0]
  void setVolumeBalance(float balance);
  void setTempo(float tempo);
  //[-12, 12]
  void setSemitone(int semitone);

  FilterProcessResult process(uint8_t *data, int64_t *size) override;

  int64_t flushRemaining() override;
  void reciveRemaining(uint8_t *data, int64_t *size) override;

private:
  FilterProcessResult applyVolume(uint8_t *data, int64_t *size);
  FilterProcessResult applyTempoAndSemitone(uint8_t *data, int64_t *size);
  void newSoundTouch();

  template <typename T>
  void forEachChannelSample(uint8_t *data, int64_t size,
                            std::function<void(T *, int)> &&func) {
    const int samples = size / m_sample_size;
    T *pcm = reinterpret_cast<T *>(data);
    for (int i = 0; i < samples; ++i) {
      func(pcm + i, i % m_config.channels);
    }
  }

  void applyU8SampleVolume(uint8_t *data, float volume);
  template <typename T, typename S>
  void applySignedSampleVolume(T *data, float volume) {
    S scaled = (S)((S)(*data) * volume);
    if (scaled > std::numeric_limits<T>::max())
      scaled = std::numeric_limits<T>::max();
    else if (scaled < std::numeric_limits<T>::lowest())
      scaled = std::numeric_limits<T>::lowest();
    *data = static_cast<T>(scaled);
  }

private:
  AudioEffectsFilterConfig m_config;
  int m_sample_size;
  std::atomic<float> m_volume;
  std::atomic<float> m_channels_volumes[10];

  SpinLock m_sound_touch_lock;
  std::unique_ptr<soundtouch::SoundTouch> m_soundtouch;
  bool m_soundtouch_flushed;
  float m_tempo;
  int m_semitone;
};
