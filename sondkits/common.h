#pragma once
#include <list>
#include <string>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

// ffplay -f s16le -ar 44100 -ch_layout stereo decode.pcm

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLE_AV_FORMAT AV_SAMPLE_FMT_S16

struct FrameData {
  uint8_t *data;
  int size;
};
using FrameDataList = std::list<FrameData>;

class DecoderInterface {
public:
  virtual ~DecoderInterface() = default;
  virtual FrameDataList decode_next_frame_data() = 0;
  virtual bool is_end() const = 0;
  virtual void free_data(uint8_t *data) = 0;
};

std::string av_err2string(int errnum);
void av_append_buffer(uint8_t **dst, int *dst_size, uint8_t *src, int src_size);
