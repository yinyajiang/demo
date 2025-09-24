#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

AVCodecID pcmAVSampleFormat2CodecId(AVSampleFormat sample_format);
