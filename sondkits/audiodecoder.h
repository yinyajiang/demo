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
  FrameDataList decodeNextFrameData() override;
  bool isEnd() const override;
  void freeData(uint8_t **data) override;
  void seek(int64_t time_ms);
  AVFormatContext *fmtCtx() const;
  AVCodecContext *codecCtx() const;
  int audioStreamIndex() const;
  double duration() const;
  int targetSampleRate() const;
  int targetChannels() const;
  AVSampleFormat targetSampleFormat() const;
  int sampleRate() const;
  int channels() const;
  AVSampleFormat sampleFormat() const;

private:
  void initSwr();
  FrameData resampleFrame(AVFrame *frame);

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
