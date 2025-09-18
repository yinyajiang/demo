#pragma once

#include <string>
#include <filesystem>
#include <functional>
extern "C" {
#include "libavformat/avformat.h"
}

class AudioDecoder;

void  foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                          std::function<bool(uint8_t *, int)> sink,
                          bool auto_free = true);
float detectAudioBPM(const std::filesystem::path& in_fpath);