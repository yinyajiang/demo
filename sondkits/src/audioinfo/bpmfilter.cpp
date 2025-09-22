#include "bpmfilter.h"
#include <cassert>
#include <memory>

#define BPM_FILTER_HOP_SIZE 96
#define BPM_FILTER_BUF_SIZE 512

BPMFilter::BPMFilter(std::shared_ptr<AudioDecoder> audio_decoder)
    : AudioThroughFilter(BPM_FILTER_HOP_SIZE * sizeof(float), true),
      m_audio_decoder(audio_decoder) {
  assert(m_audio_decoder->targetChannels() == 1);
  assert(m_audio_decoder->targetSampleFormat() == AV_SAMPLE_FMT_FLT);

  m_tempo = new_aubio_tempo("default", BPM_FILTER_BUF_SIZE, BPM_FILTER_HOP_SIZE,
                            audio_decoder->targetSampleRate());

  if (!m_tempo) {
    return;
  }

  m_input_vec = new_fvec(BPM_FILTER_HOP_SIZE);
  m_output_vec = new_fvec(2);

  if (!m_input_vec || !m_output_vec) {
    reset();
    return;
  }
}

BPMFilter::~BPMFilter() { reset(); }

void BPMFilter::reset() {
  if (m_input_vec) {
    del_fvec(m_input_vec);
    m_input_vec = nullptr;
  }
  if (m_output_vec) {
    del_fvec(m_output_vec);
    m_output_vec = nullptr;
  }
  if (m_tempo) {
    del_aubio_tempo(m_tempo);
    m_tempo = nullptr;
  }
}

void BPMFilter::throughSink(uint8_t *data, int64_t size) {
  memcpy(m_input_vec->data, data, size);
  aubio_tempo_do(m_tempo, m_input_vec, m_output_vec);
}