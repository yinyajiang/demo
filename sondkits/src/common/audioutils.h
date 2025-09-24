#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

AVCodecID pcmAVSampleFormat2CodecId(AVSampleFormat sample_format) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_FLT:
      return AV_CODEC_ID_PCM_F32LE;
    case AV_SAMPLE_FMT_S16:
      return AV_CODEC_ID_PCM_S16LE;
    case AV_SAMPLE_FMT_S32:
      return AV_CODEC_ID_PCM_S32LE;
    case AV_SAMPLE_FMT_U8:
      return AV_CODEC_ID_PCM_U8;
    default:
      return AV_CODEC_ID_NONE;
  }
}
