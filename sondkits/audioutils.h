#pragma once

#include <filesystem>
#include <functional>
#include <string>
extern "C" {
#include "libavformat/avformat.h"
}

class AudioDecoder;
void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                        std::function<bool(uint8_t *, int)> sink);


enum ChromaticKey {
  // 大调调性 (0-11)
  C_MAJOR = 0,
  G_MAJOR = 1,
  D_MAJOR = 2,
  A_MAJOR = 3,
  E_MAJOR = 4,
  B_MAJOR = 5,
  F_SHARP_MAJOR = 6,  // 等同于 G_FLAT_MAJOR
  C_SHARP_MAJOR = 7,  // 等同于 D_FLAT_MAJOR
  G_SHARP_MAJOR = 8,  // 等同于 A_FLAT_MAJOR
  D_SHARP_MAJOR = 9,  // 等同于 E_FLAT_MAJOR
  A_SHARP_MAJOR = 10, // 等同于 B_FLAT_MAJOR
  F_MAJOR = 11,

  // 小调调性 (12-23)
  A_MINOR = 12,
  E_MINOR = 13,
  B_MINOR = 14,
  F_SHARP_MINOR = 15,
  C_SHARP_MINOR = 16,
  G_SHARP_MINOR = 17,
  D_SHARP_MINOR = 18, // 等同于 E_FLAT_MINOR
  A_SHARP_MINOR = 19, // 等同于 B_FLAT_MINOR
  F_MINOR = 20,
  C_MINOR = 21,
  G_MINOR = 22,
  D_MINOR = 23,
};

int getSemitoneDifference(ChromaticKey fromKey, ChromaticKey toKey);