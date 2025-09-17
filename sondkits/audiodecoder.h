#pragma once

#include "common.h"
#include <cstdint>
#include <filesystem>
#include <list>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

class AudioDecoder : public DecoderInterface {
public:
  AudioDecoder(int target_sample_rate, int target_channels,
               AVSampleFormat target_sample_format);
  virtual ~AudioDecoder() override;

  void open(const std::filesystem::path &in_fpath);
  void close();
  FrameDataList decode_next_frame_data() override;
  bool is_end() const override;
  void free_data(uint8_t *data) override;
  void seek(int64_t time_ms);
  AVFormatContext *format_context() const;
  AVCodecContext *codec_context() const;
  int audio_stream_index() const;
  double duration() const;

private:
  void init_swr();
  FrameData resample_frame(AVFrame *frame);

private:
  AVFormatContext *m_fmt_ctx;
  AVCodecContext *m_dec_ctx;
  SwrContext *m_swr_ctx;

  // buffer
  AVPacket *m_packet;
  AVFrame *m_frame;

  int m_in_astream_idx;
  int m_target_sample_rate;
  int m_target_channels;
  int m_target_sample_size;
  AVSampleFormat m_target_sample_format;
  bool m_is_end;
};
