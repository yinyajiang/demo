#include "audioutils.h"
#include <cmath>

AVCodecID pcmSampleFmt2CodecId(AVSampleFormat sample_format) {
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

CodecOptions getPrefferCodecOptions(AVCodecID codec_id, CodecOptions hope) {
  auto codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    return hope;
  }

  AVSampleFormat* supported_fmts = nullptr;
  int supported_fmts_count = 0;
  int ret = avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&supported_fmts, &supported_fmts_count);
  if (ret == 0) {
    bool found = false;
    for (int i = 0; i < supported_fmts_count; i++) {
      if (supported_fmts[i] == hope.sample_fmt) {
        found = true;
        break;
      }
    }
    if (!found) {
      for (int i = 0; i < supported_fmts_count; i++) {
        if (av_sample_fmt_is_planar(supported_fmts[i]) == 0) {
          hope.sample_fmt = supported_fmts[i];
          break;
        }
      }
    }
  }

  int* supported_samplerates = nullptr;
  int supported_samplerates_count = 0;
  ret = avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void**)&supported_samplerates, &supported_samplerates_count);
  if (ret == 0) {
    bool found = false;
    for (int i = 0; i < supported_samplerates_count; i++) {
      if (supported_samplerates[i] == hope.sample_rate) {
        found = true;
        break;
      }
    }
    if (!found) {
      if(supported_samplerates_count > 0){
        hope.sample_rate = supported_samplerates[0];
      }
    }
  }


  AVChannelLayout* supported_channel_layouts = nullptr;
  int supported_channel_layouts_count = 0;
  ret = avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, (const void**)&supported_channel_layouts, &supported_channel_layouts_count);
  if (ret == 0) {
    bool found = false;
    for (int i = 0; i < supported_channel_layouts_count; i++) {
      if (supported_channel_layouts[i].nb_channels ==
          hope.channel_layout.nb_channels) {
        hope.channel_layout = supported_channel_layouts[i];
        found = true;
        break;
      }
    }
    if (!found) {
      if(supported_channel_layouts_count > 0){
        hope.channel_layout = supported_channel_layouts[0];
      }
    }
  }
  return hope;
}
