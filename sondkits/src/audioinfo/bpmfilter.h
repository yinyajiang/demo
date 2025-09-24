#pragma once

#include "audiothroughfilter.h"
#include <memory>
#include "audiodecoder.h"
extern "C" {
#include "aubio.h"
}

class BPMFilter: public AudioThroughFilter {
public:
    BPMFilter(int sample_rate, int channels, AVSampleFormat format);
    ~BPMFilter();
    void throughSink(uint8_t *data, int64_t size) override;
    float getBPM() const { return aubio_tempo_get_bpm(m_tempo); }
    void clear() override{};
private:
    void reset();
protected:
    aubio_tempo_t *m_tempo;
    fvec_t *m_input_vec;
    fvec_t *m_output_vec;
};