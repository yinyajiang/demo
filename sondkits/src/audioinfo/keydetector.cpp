#include "keyfinder.h"
#include "audiodata.h"
#include "workspace.h"
#include <iostream>
#include <vector>
#include "keydetector.h"



KeyDetector::KeyDetector(unsigned int sampleRate, unsigned int channels): m_sampleRate(sampleRate), m_channels(channels) {
        m_workspace.remainderBuffer.setChannels(m_channels);
        m_workspace.remainderBuffer.setFrameRate(m_sampleRate);
    }

    
    // 添加音频数据块
    void KeyDetector::addAudioData(const std::vector<float>& audioData) {

        // 创建AudioData对象
        KeyFinder::AudioData audio;
        audio.setChannels(m_channels);
        audio.setFrameRate(m_sampleRate);
        
        // 添加音频样本
        audio.addToSampleCount(audioData.size());
        for (size_t i = 0; i < audioData.size(); ++i) {
            audio.setSample(i, audioData[i]);
        }
        // 渐进式处理
        m_keyFinder.progressiveChromagram(audio, m_workspace);
    }
    
    // 获取当前key（需要足够的音频数据）
    KeyFinder::key_t KeyDetector::getCurrentKey() {
        m_keyFinder.finalChromagram(m_workspace);
        return m_keyFinder.keyOfChromagram(m_workspace);
    }
