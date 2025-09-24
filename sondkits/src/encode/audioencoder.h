#pragma once

#include <filesystem>
#include <cstdint>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#include "common.h"

struct AudioEncoderConfig {
  AVSampleFormat in_sample_format;
  int in_sample_rate;
  int in_channels;
  AVCodecID out_codec_id;
  int out_sample_rate;
  int out_channels;
  AVSampleFormat out_sample_format;
};

class AudioEncoder {
public:
  AudioEncoder();
  ~AudioEncoder();

  void open(const std::filesystem::path &file_path, AudioEncoderConfig config);
  void close();
  void encodeData(uint8_t *data, int size);
  void encodeData(AVFrame *frame);
  void flush();

private:
  void initSwr();
  AVFrame *resample2Frame(uint8_t *data, int size);
  AVFrame *flushSwr();
private:
  AVCodecContext *m_codec_ctx;
  AVFormatContext *m_format_ctx;
  AudioEncoderConfig m_config;
  SwrContext *m_swr_ctx;
  AVFrame *m_frame;
  AVPacket *m_packet;
  int64_t m_next_pts;
  SpinLock m_lock;
};