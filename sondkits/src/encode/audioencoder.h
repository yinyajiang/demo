#pragma once

#include <cstdint>
#include <filesystem>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}
#include "common.h"


class AudioEncoder {
public:
  AudioEncoder();
  ~AudioEncoder();

  void open(const std::filesystem::path &file_path, int in_sample_rate, int in_channels, AVSampleFormat in_sample_format);
  void close();
  void encodeData(uint8_t *data, int size);
  bool flush();
private:
  void initSwr();
  int resample(uint8_t *data, int size, uint8_t** out_data);
  int flushSwr(uint8_t** out_data);

  void intoFifoEncodeData(uint8_t **sample_data, int nb_samples);
  bool encodeFrame(AVFrame *frame);
private:
  AVCodecContext *m_codec_ctx;
  AVFormatContext *m_format_ctx;
  int m_in_sample_rate;
  int m_in_channels;
  AVSampleFormat m_in_sample_format;
  AVFrame *m_frame;
  AVPacket *m_packet;
  int64_t m_next_pts;
  SpinLock m_lock;
  SwrContext *m_swr_ctx;
  struct AVAudioFifo* m_fifo;
};