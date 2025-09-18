#include "audioutils.h"
#include "audiodecoder.h"
#include "common.h"
#include <filesystem>
#include <functional>
#include "BPMDetect.h"

void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder, std::function<bool(uint8_t*, int)> sink, bool auto_free) {
  if (!audio_decoder || !sink) {
    return;
  }
  while (!audio_decoder->isEnd()) {
    auto frame_data = audio_decoder->decodeNextFrameData();
    for (auto &frame : frame_data) {
      auto con = sink(frame.data, frame.size);
      if (auto_free) {
        audio_decoder->freeData(&frame.data);
      }
    }
  }
}

float detectAudioBPM(const std::filesystem::path &in_fpath) {
    const int channels = 1;
    auto audio_decoder = std::make_shared<AudioDecoder>(DEFAULT_SAMPLE_RATE, channels, AV_SAMPLE_FMT_FLT);
    audio_decoder->open(in_fpath);
    soundtouch::BPMDetect bpm(1, DEFAULT_SAMPLE_RATE);

    foreachDecoderData(audio_decoder, [&bpm](uint8_t *data, int size) {
      auto num_samples = size / sizeof(soundtouch::SAMPLETYPE) / channels;
      bpm.inputSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data), num_samples);
      return true;
    });
    audio_decoder->close();
    return  bpm.getBpm();

}
