#include "audioutils.h"
#include "BPMDetect.h"
#include "audiodecoder.h"
#include "common.h"
#include <filesystem>
#include <functional>

void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                        std::function<bool(uint8_t *, int)> sink,
                        bool auto_free) {
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
  auto audio_decoder = std::make_shared<AudioDecoder>(
      DEFAULT_SAMPLE_RATE, channels, AV_SAMPLE_FMT_FLT);
  audio_decoder->open(in_fpath);
  soundtouch::BPMDetect bpm(1, DEFAULT_SAMPLE_RATE);

  foreachDecoderData(audio_decoder, [&bpm](uint8_t *data, int size) {
    auto num_samples = size / sizeof(soundtouch::SAMPLETYPE) / channels;
    bpm.inputSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data),
                     num_samples);
    return true;
  });
  audio_decoder->close();
  return bpm.getBpm();
}

int getSemitoneDifference(ChromaticKey fromKey, ChromaticKey toKey) {
  // 将调性转换为对应的根音半音值
  // 大调：0-11，小调：12-23，但小调需要转换为对应的根音
  int fromRoot, toRoot;

  if (fromKey < 12) {
    // 大调调性，直接使用枚举值
    fromRoot = fromKey;
  } else {
    // 小调调性，转换为对应的根音
    // 小调调性的根音 = 大调调性 + 9个半音（小三度）
    fromRoot = (fromKey - 12 + 9) % 12;
  }

  if (toKey < 12) {
    // 大调调性
    toRoot = toKey;
  } else {
    // 小调调性
    toRoot = (toKey - 12 + 9) % 12;
  }

  // 计算半音差
  int semitoneDifference = (toRoot - fromRoot) % 12;
  if (semitoneDifference < 0)
    semitoneDifference += 12;
  return semitoneDifference;
}
