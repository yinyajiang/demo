#include "composedatasource.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
#include <type_traits>
#include <QDebug>

ComposeDataSource::ComposeDataSource(int64_t frame_size)
    : DataSource(frame_size) {
}

void ComposeDataSource::addDataSource(std::shared_ptr<DataSource> data_source) {
    if (!data_source) {
        return;
    }
    m_data_sources.push_back(data_source);
}


int64_t ComposeDataSource::bytesAvailable() const {
    if (m_data_sources.empty()) {
        return 0;
    }
    int64_t max_bytes = 0;
    for (const auto& source : m_data_sources) {
        max_bytes = std::max(max_bytes, source->bytesAvailable());
    }
    return max_bytes;
}

int64_t ComposeDataSource::realReadData(uint8_t *data, int64_t max_size) {
    if (!data || max_size <= 0 || m_data_sources.empty()) {
        return 0;
    }

    auto it = m_data_sources.begin();
    int64_t read_size = 0;
    while(it != m_data_sources.end()) {
        read_size = (*it)->readData(data, max_size);
        ++it;
        if (read_size > 0) {
            break;
        }
    }
    if(read_size <= 0 || it == m_data_sources.end()) {
        return read_size;
    }

    std::vector<uint8_t> temp_buffer(read_size);
    while(it != m_data_sources.end()) {
        auto size = (*it)->readData(temp_buffer.data(), read_size);
        if (size != read_size) {
            continue;
        }
        mixAudioData<float>(reinterpret_cast<float*>(data), 
                     reinterpret_cast<const float*>(temp_buffer.data()), 
                     read_size / sizeof(float), 1.0f, ComposeDataSource::MIX_MODE_SOFT_CLIP);
        it++;
    }
    return read_size;
}

bool ComposeDataSource::realIsEnd() const {
    if (m_data_sources.empty()) {
        return true;
    }
    for (const auto& source : m_data_sources) {
        if (!source->isEnd()) {
            return false;
        }
    }
    return true;
}

void ComposeDataSource::realClear() {
    for (auto& source : m_data_sources) {
        if (source) {
            source->clear();
        }
    }
}

// 混音算法实现
template <typename T>
void ComposeDataSource::mixAudioData(T *dest, const T *src, int64_t size, float volume, ComposeDataSource::MixMode mode) {
    if (!dest || !src || size <= 0) {
        return;
    }
    
    for (int64_t i = 0; i < size; ++i) {
        T src_sample = static_cast<T>(src[i] * volume);
        
        switch (mode) {
        case MIX_MODE_ADD: {
            // 简单相加混音 - 最常用的方法
            dest[i] += src_sample;
            break;
        }
        case MIX_MODE_AVERAGE: {
            // 平均混音 - 避免音量过大
            dest[i] = (dest[i] + src_sample) * 0.5f;
            break;
        }
        case MIX_MODE_MULTIPLY: {
            // 相乘混音 - 产生调制效果
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
        case MIX_MODE_VOCAL_MUSIC: {
            // 人声音乐专用混音 - 针对分离后重新合并优化
            if constexpr (std::is_floating_point_v<T>) {
                // 假设dest是音乐，src是人声
                T music_sample = dest[i];
                T vocal_sample = src_sample;
                
                // 动态压缩以防止峰值
                T combined = music_sample + vocal_sample;
                
                // 使用软膝压缩算法
                T threshold = 0.7f;  // 压缩阈值
                T ratio = 0.25f;     // 压缩比（4:1）
                
                if (std::abs(combined) > threshold) {
                    T over_threshold = std::abs(combined) - threshold;
                    T compressed_gain = threshold + over_threshold * ratio;
                    T gain_factor = compressed_gain / std::abs(combined);
                    dest[i] = combined * gain_factor;
                } else {
                    dest[i] = combined;
                }
                
                // 添加轻微的谐波增强
                dest[i] *= (1.0f + 0.05f * std::sin(i * 0.1f));
            } else {
                // 整数类型回退到软限幅
                T mixed = dest[i] + src_sample;
                auto max_val = std::numeric_limits<T>::max();
                auto min_val = std::numeric_limits<T>::lowest();
                dest[i] = std::clamp(mixed, min_val, max_val);
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

// 显式实例化模板函数，支持常用的音频格式
template void ComposeDataSource::mixAudioData<float>(float*, const float*, int64_t, float, ComposeDataSource::MixMode);
template void ComposeDataSource::mixAudioData<int16_t>(int16_t*, const int16_t*, int64_t, float, ComposeDataSource::MixMode);
template void ComposeDataSource::mixAudioData<int32_t>(int32_t*, const int32_t*, int64_t, float, ComposeDataSource::MixMode);
