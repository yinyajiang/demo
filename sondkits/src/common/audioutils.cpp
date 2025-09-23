#include "audioutils.h"
#include "decode/audiodecoder.h"
#include "decodedatasource.h"
#include "decodequeue.h"
#include <functional>
extern "C" {
#include <libavutil/avutil.h>
}

void foreachDecoderData(std::shared_ptr<AudioDecoder> audio_decoder,
                        std::function<bool(uint8_t *, int64_t)> sink,
                        int64_t min_sink_size, int64_t max_sink_size) {
  if (!audio_decoder || !sink) {
    return;
  }

  int64_t frame_size =
      audio_decoder->targetChannels() *
      av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();
  DecodeDataSource source(nullptr, frame_size, decode_queue);


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
      int r = source.readData(buffer + readed, buffer_size - readed);
      if (r > 0) {
        readed += r;
      }
    }
    if (readed > 0) {
      auto con = sink(buffer, readed);
      if (!con) {
        break;
      }
    }
  }
  av_freep(&buffer);
  decode_queue->stop();
}