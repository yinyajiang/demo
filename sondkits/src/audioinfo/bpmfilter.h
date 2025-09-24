#pragma once

#include "audiodecoder.h"
#include "audiothroughfilter.h"
#include <memory>
extern "C" {
#include "aubio.h"
}

class BPMFilter : public AudioThroughFilter {
public:
  BPMFilter(int sample_rate, int channels, AVSampleFormat format);
  ~BPMFilter();
  float getBPM() const { return aubio_tempo_get_bpm(m_tempo); }

protected:
  void throughSink(uint8_t *data, int64_t size) override;

private:
  void reset();

protected:
  aubio_tempo_t *m_tempo;
  fvec_t *m_input_vec;
  fvec_t *m_output_vec;
};