#pragma once

#include "audioencoder.h"
#include "audiothroughfilter.h"
#include <memory>

class EncodeFilter : public AudioThroughFilter {
public:
  EncodeFilter(std::shared_ptr<AudioEncoder> audio_encoder);
  ~EncodeFilter();

protected:
  void throughSink(uint8_t *data, int64_t size) override;

private:
  std::shared_ptr<AudioEncoder> m_audio_encoder;
};