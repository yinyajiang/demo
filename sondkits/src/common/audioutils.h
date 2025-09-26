#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

AVCodecID pcmSampleFmt2CodecId(AVSampleFormat sample_format);

struct CodecOptions {
  AVSampleFormat sample_fmt;
  int sample_rate;
  AVChannelLayout channel_layout;
};

CodecOptions getPrefferCodecOptions(AVCodecID codec_id, CodecOptions hope);