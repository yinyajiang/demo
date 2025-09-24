#include "keyfilter.h"
#include <cassert>
#include <memory>

#define KEY_FILTER_HOP_SIZE 96

KeyFilter::KeyFilter(int sample_rate, int channels, AVSampleFormat format)
    : AudioThroughFilter(KEY_FILTER_HOP_SIZE * sizeof(float), false) {
  assert(channels == 1);
  assert(format == AV_SAMPLE_FMT_FLT);

  m_sampleRate = sample_rate;

  // m_audioData.setChannels(1);
  // m_audioData.setFrameRate(m_sampleRate);
}

KeyFilter::~KeyFilter() {}

void KeyFilter::throughSink(uint8_t *data, int64_t size) {
  // auto num_samples = size / sizeof(float);
  // if (num_samples == 0) {
  //   return;
  // }
  // if (m_audioData.getSampleCount() == 0) {
  //   m_audioData.addToSampleCount(num_samples);
  // }

  // float *float_data = reinterpret_cast<float *>(data);
  // for (size_t i = 0; i < num_samples; ++i) {
  //   m_audioData.setSample(i, float_data[i]);
  // }
  // m_keyFinder.progressiveChromagram(m_audioData, m_workspace);
}

KeyFinder::key_t KeyFilter::getKey() {
  // m_keyFinder.finalChromagram(m_workspace);
  // return m_keyFinder.keyOfChromagram(m_workspace);
  return KeyFinder::key_t::A_MAJOR;
}

std::string KeyFilter::keyToString(KeyFinder::key_t key) {
  switch (key) {
  case KeyFinder::A_MAJOR:
    return "A Major";
  case KeyFinder::A_MINOR:
    return "A Minor";
  case KeyFinder::B_FLAT_MAJOR:
    return "Bb Major";
  case KeyFinder::B_FLAT_MINOR:
    return "Bb Minor";
  case KeyFinder::B_MAJOR:
    return "B Major";
  case KeyFinder::B_MINOR:
    return "B Minor";
  case KeyFinder::C_MAJOR:
    return "C Major";
  case KeyFinder::C_MINOR:
    return "C Minor";
  case KeyFinder::D_FLAT_MAJOR:
    return "Db Major";
  case KeyFinder::D_FLAT_MINOR:
    return "Db Minor";
  case KeyFinder::D_MAJOR:
    return "D Major";
  case KeyFinder::D_MINOR:
    return "D Minor";
  case KeyFinder::E_FLAT_MAJOR:
    return "Eb Major";
  case KeyFinder::E_FLAT_MINOR:
    return "Eb Minor";
  case KeyFinder::E_MAJOR:
    return "E Major";
  case KeyFinder::E_MINOR:
    return "E Minor";
  case KeyFinder::F_MAJOR:
    return "F Major";
  case KeyFinder::F_MINOR:
    return "F Minor";
  case KeyFinder::G_FLAT_MAJOR:
    return "Gb Major";
  case KeyFinder::G_FLAT_MINOR:
    return "Gb Minor";
  case KeyFinder::G_MAJOR:
    return "G Major";
  case KeyFinder::G_MINOR:
    return "G Minor";
  case KeyFinder::A_FLAT_MAJOR:
    return "Ab Major";
  case KeyFinder::A_FLAT_MINOR:
    return "Ab Minor";
  case KeyFinder::SILENCE:
    return "Silence";
  default:
    return "Unknown";
  }
}
