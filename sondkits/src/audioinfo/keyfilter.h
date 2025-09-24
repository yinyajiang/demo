#pragma once

#include "audiodecoder.h"
#include "audiothroughfilter.h"
#include "keyfinder.h"
#include "workspace.h"
#include <memory>
#include <string>

class KeyFilter : public AudioThroughFilter {
public:
  KeyFilter(int sample_rate, int channels, AVSampleFormat format);
  ~KeyFilter();

  KeyFinder::key_t getKey();
  static std::string keyToString(KeyFinder::key_t key);

protected:
  void throughSink(uint8_t *data, int64_t size) override;

private:
  int m_sampleRate;
  // KeyFinder::KeyFinder m_keyFinder;
  // KeyFinder::Workspace m_workspace;
  // KeyFinder::AudioData m_audioData;
};