#pragma once

#include <cstdint>
#include <filesystem>
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
  AVCodecID out_codec_id = AV_CODEC_ID_NONE;
  int out_sample_rate = WORKING_SAMPLE_RATE();
  int out_channels = WORKING_CHANNELS;
  AVSampleFormat out_sample_format = WORKING_SAMPLE_AV_FORMAT;
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

  // 状态查询方法
  bool isOpen() const {
    return m_codec_ctx != nullptr && m_format_ctx != nullptr;
  }

private:
  void initSwr();
  AVFrame *resample2Frame(uint8_t *data, int size);
  AVFrame *flushSwr();

private:
  AVCodecContext *m_codec_ctx;
  AVFormatContext *m_format_ctx;
  AudioEncoderConfig m_config;
  AVFrame *m_frame;
  AVPacket *m_packet;
  int64_t m_next_pts;
  SpinLock m_lock;
  SwrContext *m_swr_ctx;
  std::vector<uint8_t *> m_swr_in_data;
};