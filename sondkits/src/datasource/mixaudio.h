#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

class MixAudio {
public:
  enum MixMode {
    MIX_MODE_AVERAGE,       // 平均混音
    MIX_MODE_MULTIPLY,      // 相乘混音
    MIX_MODE_SOFT_CLIP,     // 软限幅混音
    MIX_MODE_HARMONIC,      // 谐波混音
    MIX_MODE_SPECTRAL_BLEND // 频谱混合
  };

  template <typename T>
  void mix(uint8_t *dest, const uint8_t *src, int64_t size, MixMode mode) {
    mixAudioData<T>(dest, src, size, mode);
  }

private:
  template <typename T>
  void mixAudioData(uint8_t *dest_, const uint8_t *src_, int64_t size_,
                    MixMode mode) {
    if (!dest_ || !src_ || size_ <= 0) {
      return;
    }
    T *dest = reinterpret_cast<T *>(dest_);
    const T *src = reinterpret_cast<const T *>(src_);
    int size = size_ / sizeof(T);
    for (int i = 0; i < size; ++i) {
      T src_sample = static_cast<T>(src[i]);

      switch (mode) {
      case MIX_MODE_AVERAGE: {
        dest[i] = dest[i] * 0.5f + src_sample * 0.5f;
        break;
      }
      case MIX_MODE_MULTIPLY: {
        dest[i] *= src_sample;
        break;
      }
      case MIX_MODE_SOFT_CLIP: {
        // 软限幅混音 - 防止削波失真
        T mixed = dest[i] + src_sample;
        if constexpr (std::is_floating_point_v<T>) {
          // 对于浮点数使用tanh函数进行软限幅
          if (std::abs(mixed) > 1.0f) {
            dest[i] = std::tanh(mixed);
          } else {
            dest[i] = mixed;
          }
        } else {
          // 对于整数类型进行范围限制
          auto max_val = std::numeric_limits<T>::max();
          auto min_val = std::numeric_limits<T>::lowest();
          if (mixed > max_val) {
            dest[i] = max_val;
          } else if (mixed < min_val) {
            dest[i] = min_val;
          } else {
            dest[i] = mixed;
          }
        }
        break;
      }
      case MIX_MODE_HARMONIC: {
        // 谐波混音 - 增强谐波成分
        if constexpr (std::is_floating_point_v<T>) {
          T mixed = dest[i] + src_sample;
          // 添加二次谐波增强
          T harmonic = mixed * mixed * 0.1f;
          dest[i] = std::tanh(mixed + harmonic);
        } else {
          dest[i] += src_sample;
        }
        break;
      }
      case MIX_MODE_SPECTRAL_BLEND: {
        // 频谱混合 - 基于能量的智能混合
        if constexpr (std::is_floating_point_v<T>) {
          T music_energy = dest[i] * dest[i];
          T vocal_energy = src_sample * src_sample;
          T total_energy = music_energy + vocal_energy + 1e-6f;

          // 基于能量比例的混合
          T music_weight = music_energy / total_energy;
          T vocal_weight = vocal_energy / total_energy;

          dest[i] = dest[i] * music_weight + src_sample * vocal_weight;
        } else {
          dest[i] = (dest[i] + src_sample) * 0.5f;
        }
        break;
      }
      default:
        dest[i] += src_sample;
        break;
      }
    }
  }
};
