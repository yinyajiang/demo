#include "audioutils.h"
#include "BPMDetect.h"
#include "audiodecoder.h"
#include "common.h"
#include <filesystem>
#include <functional>
#include "datasource.h"
#include "decodequeue.h"
extern "C" {
#include <libavutil/avutil.h>
#include "aubio.h"
}

void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                        std::function<bool(uint8_t *, int64_t)> sink,
                        int64_t min_sink_size,
                        int64_t max_sink_size
                        ) {
  if (!audio_decoder || !sink) {
    return;
  }

  int64_t frame_size = audio_decoder->targetChannels() * av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue = std::make_shared<DecodeQueue>(audio_decoder);
  DecodeDataSource source(nullptr, frame_size, decode_queue);
  source.open();

  if (min_sink_size <= 0) {
    min_sink_size = frame_size;
  }
  if (max_sink_size <= 0) {
    max_sink_size = frame_size * 1024;
  }

  int64_t frame_count = max_sink_size / frame_size;
  if (frame_count <= 0) {
    return;
  }
  int64_t buffer_size = frame_size * frame_count;
  buffer_size = std::min(buffer_size, max_sink_size);

  uint8_t *buffer = (uint8_t *)av_mallocz(buffer_size);
  while (!source.isEnd()) {
    int readed = 0;
    while (!source.isEnd() && readed < min_sink_size) {
      int r = source.readData(buffer+readed, buffer_size-readed);
      if (r > 0) {
        readed += r;
      }
    }
    if (readed>0) {
        auto con = sink(buffer, readed);
        if (!con) {
        break;
        }
    }
  }
  av_freep(&buffer);
  source.close();
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

