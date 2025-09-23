#pragma once
#include "datasource.h"
#include <vector>
#include <memory>
#include <mutex>

class ComposeDataSource : public DataSource {
public:
    // 混音模式枚举
    enum MixMode {
        MIX_MODE_ADD,           // 简单相加混音
        MIX_MODE_AVERAGE,       // 平均混音
        MIX_MODE_MULTIPLY,      // 相乘混音
        MIX_MODE_SOFT_CLIP,     // 软限幅混音
        MIX_MODE_VOCAL_MUSIC,   // 人声音乐专用混音（推荐）
        MIX_MODE_HARMONIC,      // 谐波混音
        MIX_MODE_SPECTRAL_BLEND // 频谱混合
    };

    explicit ComposeDataSource(int64_t frame_size);
    ~ComposeDataSource() override = default;

    void addDataSource(std::shared_ptr<DataSource> data_source);

    // 获取可用字节数
    int64_t bytesAvailable() const override;

protected:
    int64_t realReadData(uint8_t *data, int64_t max_size) override;
    bool realIsEnd() const override;
    void realClear() override;

private:
    std::vector<std::shared_ptr<DataSource>> m_data_sources;
    
    // 混音函数，支持多种混音算法
    template <typename T> 
    void mixAudioData(T *dest, const T *src, int64_t size, float volume = 1.0f, MixMode mode = MIX_MODE_ADD);
};
