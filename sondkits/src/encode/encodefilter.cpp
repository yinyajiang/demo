#include "encodefilter.h"

EncodeFilter::EncodeFilter(std::shared_ptr<AudioEncoder> audio_encoder)
    : AudioThroughFilter(false), m_audio_encoder(audio_encoder) {}

EncodeFilter::~EncodeFilter() {}

void EncodeFilter::throughSink(uint8_t *data, int64_t size) {
  m_audio_encoder->encodeData(data, size);
}