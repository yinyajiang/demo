#pragma once
#include "keyfinder.h"
#include "workspace.h"


class KeyDetector {
public:
    KeyDetector(unsigned int sampleRate, unsigned int channels);
    void addAudioData(const std::vector<float>& audioData);
    KeyFinder::key_t getCurrentKey();
private:
    KeyFinder::KeyFinder m_keyFinder;
    KeyFinder::Workspace m_workspace;

    unsigned int m_sampleRate;
    unsigned int m_channels;
};
