#pragma once

#include <functional>
#include <memory>

class AudioDecoder;
void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                        std::function<bool(uint8_t *, int64_t)> sink,
                        int64_t min_sink_size = 0, int64_t max_sink_size = 0);