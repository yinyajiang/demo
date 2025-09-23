#pragma once

#include "audiodecoder.h"
#include "audiothroughfilter.h"
#include "keyfinder.h"
#include "workspace.h"
#include <memory>
#include <string>

class KeyFilter : public AudioThroughFilter {
public:
  KeyFilter(std::shared_ptr<AudioDecoder> audio_decoder);
  ~KeyFilter();
  void clear() override{};
  void throughSink(uint8_t *data, int64_t size) override;
  KeyFinder::key_t getKey();

  // 辅助函数：将 key_t 转换为可读字符串
  static std::string keyToString(KeyFinder::key_t key);

protected:
  int m_sampleRate;
  // KeyFinder::KeyFinder m_keyFinder;
  // KeyFinder::Workspace m_workspace;
  // KeyFinder::AudioData m_audioData;
};