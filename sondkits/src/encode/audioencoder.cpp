#include "audioencoder.h"
#include <cctype>
#include <cstddef>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}
#include "audioutils.h"
#include "common.h"
#include <iostream>
#include <mutex>

AudioEncoder::AudioEncoder()
    : m_codec_ctx(nullptr), m_format_ctx(nullptr), m_swr_ctx(nullptr),
      m_frame(nullptr), m_packet(nullptr), m_next_pts(0), m_fifo(nullptr) {}

AudioEncoder::~AudioEncoder() { close(); }

void AudioEncoder::open(const std::filesystem::path &file_path,
                        AudioEncoderConfig config) {

  // 验证配置
  if (config.in_sample_rate <= 0 || config.in_channels <= 0 ||
      config.out_sample_rate <= 0 || config.out_channels <= 0 ||
      config.in_sample_format == AV_SAMPLE_FMT_NONE ||
      config.out_sample_format == AV_SAMPLE_FMT_NONE) {
    throw std::runtime_error("Invalid AudioEncoderConfig");
  }

  std::unique_lock<SpinLock> lock(m_lock);
  m_next_pts = 0;

  int ret = avformat_alloc_output_context2(&m_format_ctx, nullptr, nullptr,
                                           fs2u8(file_path).c_str());
  if (ret < 0) {
    throw std::runtime_error("[avformat_alloc_output_context2]: " +
                             avErr2String(ret));
  }

  if (config.out_codec_id == AV_CODEC_ID_NONE) {
    std::string extension = toLower(file_path.extension().string());
    if (extension == ".wav") {
      config.out_codec_id = pcmAVSampleFormat2CodecId(config.in_sample_format);
    } else if (extension == ".mp3") {
      config.out_codec_id = AV_CODEC_ID_MP3;
    } else {
      config.out_codec_id = AV_CODEC_ID_AAC;
    }
  }
  auto codec = avcodec_find_encoder(config.out_codec_id);
  if (!codec) {
    throw std::runtime_error("[avcodec_find_encoder]: " +
                             std::to_string(config.out_codec_id));
  }

  AVStream *stream = avformat_new_stream(m_format_ctx, nullptr);
  if (!stream) {
    throw std::runtime_error("[avformat_new_stream]: " +
                             std::to_string(config.out_codec_id));
  }

  m_codec_ctx = avcodec_alloc_context3(codec);
  if (!m_codec_ctx) {
    throw std::runtime_error("Failed to allocate codec context");
  }
  m_config = config;
  m_codec_ctx->sample_fmt = config.out_sample_format;
  m_codec_ctx->sample_rate = config.out_sample_rate;
  av_channel_layout_default(&m_codec_ctx->ch_layout, config.out_channels);

  // Ensure encoder uses supported sample format / rate / layout
  if (codec->sample_fmts) {
    bool supported = false;
    for (const enum AVSampleFormat *p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
      if (*p == m_codec_ctx->sample_fmt) { supported = true; break; }
    }
    if (!supported) {
      // Prefer FLTP when available (MP3/AAC typical)
      enum AVSampleFormat preferred = AV_SAMPLE_FMT_FLTP;
      bool foundPreferred = false;
      for (const enum AVSampleFormat *p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
        if (*p == preferred) { m_codec_ctx->sample_fmt = *p; foundPreferred = true; break; }
      }
      if (!foundPreferred) {
        m_codec_ctx->sample_fmt = codec->sample_fmts[0];
      }
    }
  }

  if (codec->supported_samplerates) {
    int desired = m_codec_ctx->sample_rate;
    int best = codec->supported_samplerates[0];
    int bestDiff = std::abs(best - desired);
    for (const int *p = codec->supported_samplerates; *p; ++p) {
      int d = std::abs(*p - desired);
      if (d < bestDiff) { best = *p; bestDiff = d; }
    }
    m_codec_ctx->sample_rate = best;
  }

  if (codec->ch_layouts) {
    // Try to keep requested channels if possible
    const AVChannelLayout *chosen = &codec->ch_layouts[0];
    for (const AVChannelLayout *p = codec->ch_layouts; p && p->nb_channels; ++p) {
      if ((int)p->nb_channels == config.out_channels) { chosen = p; break; }
    }
    av_channel_layout_copy(&m_codec_ctx->ch_layout, chosen);
  }

  // Sync effective encoder params back to config for downstream logic
  m_config.out_sample_format = m_codec_ctx->sample_fmt;
  m_config.out_sample_rate = m_codec_ctx->sample_rate;
  m_config.out_channels = m_codec_ctx->ch_layout.nb_channels;
  
  m_codec_ctx->time_base = av_make_q(1, m_codec_ctx->sample_rate);
  m_codec_ctx->pkt_timebase = m_codec_ctx->time_base;
  if (config.out_codec_id == AV_CODEC_ID_MP3) {
    m_codec_ctx->bit_rate = ENCODER_MP3_BIT_RATE;
  }
  if (m_format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
    m_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  m_frame = av_frame_alloc();
  if (!m_frame) {
    throw std::runtime_error("[av_frame_alloc]");
  }

  m_packet = av_packet_alloc();
  if (!m_packet) {
    throw std::runtime_error("[av_packet_alloc]");
  }
  m_frame->format = m_codec_ctx->sample_fmt;
  m_frame->ch_layout = m_codec_ctx->ch_layout;
  m_frame->sample_rate = m_codec_ctx->sample_rate;
  m_next_pts = 0;

  auto codec_ret = avcodec_open2(m_codec_ctx, codec, nullptr);
  if (codec_ret < 0) {
    throw std::runtime_error("[avcodec_open2]: " +
                             avErr2String(codec_ret));
  }
  auto param_ret =
      avcodec_parameters_from_context(stream->codecpar, m_codec_ctx);
  if (param_ret < 0) {
    throw std::runtime_error("[avcodec_parameters_from_context]: " +
                             avErr2String(param_ret));
  }
  stream->time_base = m_codec_ctx->time_base;

  if (!(m_format_ctx->oformat->flags & AVFMT_NOFILE)) {

    if (!std::filesystem::exists(file_path.parent_path())) {
      std::filesystem::create_directories(file_path.parent_path());
    }

    auto io_ret =
        avio_open(&m_format_ctx->pb, fs2u8(file_path).c_str(), AVIO_FLAG_WRITE);
    if (io_ret < 0) {
      throw std::runtime_error("[avio_open]: " +
                               avErr2String(io_ret));
    }
  }

  auto header_ret = avformat_write_header(m_format_ctx, nullptr);
  if (header_ret < 0) {
    throw std::runtime_error("[avformat_write_header]: " +
                             avErr2String(header_ret));
  }

  if (m_config.out_channels == m_config.in_channels &&
      m_config.out_sample_rate == m_config.in_sample_rate &&
      m_config.out_sample_format == m_config.in_sample_format) {
    m_swr_ctx = nullptr;
  } else {
    initSwr();
  }
}

void AudioEncoder::close() {
  std::unique_lock<SpinLock> lock(m_lock);
  if (m_format_ctx) {
    avformat_free_context(m_format_ctx);
    m_format_ctx = nullptr;
  }
  if (m_codec_ctx) {
    avcodec_free_context(&m_codec_ctx);
    m_codec_ctx = nullptr;
  }
  if (m_swr_ctx) {
    swr_free(&m_swr_ctx);
    m_swr_ctx = nullptr;
  }
  if (m_fifo) {
    av_audio_fifo_free(m_fifo);
    m_fifo = nullptr;
  }
  if (m_frame) {
    av_frame_free(&m_frame);
    m_frame = nullptr;
  }
  if (m_packet) {
    av_packet_free(&m_packet);
    m_packet = nullptr;
  }
}

void AudioEncoder::encodeData(uint8_t *data, int size) {
  if (!data || size <= 0) {
    return;
  }
  {
    std::unique_lock<SpinLock> lock(m_lock);
    if (!m_codec_ctx || !m_format_ctx) {
      return;
    }
  }
  AVFrame *frame = resample2Frame(data, size);
  encodeData(frame);
}

void AudioEncoder::encodeData(AVFrame *frame) {
  if (!frame) {
    return;
  }
  std::unique_lock<SpinLock> lock(m_lock);
  if (!m_codec_ctx || !m_format_ctx || !m_packet ||
      m_format_ctx->nb_streams == 0) {
    return;
  }
  // Ensure frame.nb_samples satisfies encoder's required frame_size (e.g., MP3 1152)
  int required = (m_codec_ctx->frame_size > 0) ? m_codec_ctx->frame_size : frame->nb_samples;
  if (!m_fifo) {
    m_fifo = av_audio_fifo_alloc((AVSampleFormat)m_codec_ctx->sample_fmt,
                                 m_codec_ctx->ch_layout.nb_channels,
                                 required);
  }
  // write incoming samples into FIFO
  if (av_audio_fifo_write(m_fifo, (void**)frame->data, frame->nb_samples) < frame->nb_samples) {
    std::cerr << "fifo write failed" << std::endl;
    return;
  }
  while (av_audio_fifo_size(m_fifo) >= required) {
    AVFrame *out = av_frame_alloc();
    out->format = m_codec_ctx->sample_fmt;
    out->ch_layout = m_codec_ctx->ch_layout;
    out->sample_rate = m_codec_ctx->sample_rate;
    out->nb_samples = required;
    if (av_frame_get_buffer(out, 0) < 0) { av_frame_free(&out); break; }
    if (av_frame_make_writable(out) < 0) { av_frame_free(&out); break; }
    if (av_audio_fifo_read(m_fifo, (void**)out->data, required) < required) { av_frame_free(&out); break; }
    out->pts = m_next_pts;
    m_next_pts += av_rescale_q(out->nb_samples, av_make_q(1, m_codec_ctx->sample_rate), m_codec_ctx->time_base);

    auto ret = avcodec_send_frame(m_codec_ctx, out);
    av_frame_free(&out);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      std::cerr << "Error sending frame: " << avErr2String(ret) << std::endl;
      break;
    }
    while (m_codec_ctx) {
      auto packet_ret = avcodec_receive_packet(m_codec_ctx, m_packet);
      if (packet_ret == AVERROR(EAGAIN) || packet_ret == AVERROR_EOF) {
        break;
      } else if (packet_ret < 0) {
        std::cerr << "Error receiving packet: " << avErr2String(packet_ret)
                  << std::endl;
        break;
      }
      av_packet_rescale_ts(m_packet, m_codec_ctx->time_base,
                           m_format_ctx->streams[0]->time_base);
      m_packet->stream_index = m_format_ctx->streams[0]->index;
      auto write_ret = av_interleaved_write_frame(m_format_ctx, m_packet);
      if (write_ret < 0) {
        std::cerr << "Error writing packet: " << avErr2String(write_ret)
                  << std::endl;
        break;
      }
      av_packet_unref(m_packet);
    }
  }
}

void AudioEncoder::flush() {
  if (!m_codec_ctx) {
    return;
  }
  AVFrame *frame = flushSwr();
  if (frame) {
    encodeData(frame);
  }
  // Drain remaining samples in FIFO by padding zeros to full frames
  if (m_fifo && av_audio_fifo_size(m_fifo) > 0) {
    int required = (m_codec_ctx->frame_size > 0) ? m_codec_ctx->frame_size : av_audio_fifo_size(m_fifo);
    while (av_audio_fifo_size(m_fifo) > 0) {
      int to_read = std::min(required, av_audio_fifo_size(m_fifo));
      AVFrame *out = av_frame_alloc();
      out->format = m_codec_ctx->sample_fmt;
      out->ch_layout = m_codec_ctx->ch_layout;
      out->sample_rate = m_codec_ctx->sample_rate;
      out->nb_samples = required;
      if (av_frame_get_buffer(out, 0) < 0) { av_frame_free(&out); break; }
      if (av_frame_make_writable(out) < 0) { av_frame_free(&out); break; }
      // read available samples
      if (av_audio_fifo_read(m_fifo, (void**)out->data, to_read) < to_read) { av_frame_free(&out); break; }
      // zero-pad remaining if needed
      if (to_read < required) {
        int planes = av_sample_fmt_is_planar((AVSampleFormat)m_codec_ctx->sample_fmt) ? m_codec_ctx->ch_layout.nb_channels : 1;
        int bytes_per_sample = av_get_bytes_per_sample((AVSampleFormat)m_codec_ctx->sample_fmt);
        for (int p = 0; p < planes; ++p) {
          int offset = to_read * (av_sample_fmt_is_planar((AVSampleFormat)m_codec_ctx->sample_fmt) ? 1 : m_codec_ctx->ch_layout.nb_channels) * bytes_per_sample;
          int pad = (required - to_read) * (av_sample_fmt_is_planar((AVSampleFormat)m_codec_ctx->sample_fmt) ? 1 : m_codec_ctx->ch_layout.nb_channels) * bytes_per_sample;
          memset(out->data[p] + offset, 0, pad);
        }
      }
      out->pts = m_next_pts;
      m_next_pts += av_rescale_q(out->nb_samples, av_make_q(1, m_codec_ctx->sample_rate), m_codec_ctx->time_base);
      auto ret = avcodec_send_frame(m_codec_ctx, out);
      av_frame_free(&out);
      if (ret < 0 && ret != AVERROR(EAGAIN)) { break; }
      while (m_codec_ctx) {
        auto packet_ret = avcodec_receive_packet(m_codec_ctx, m_packet);
        if (packet_ret == AVERROR(EAGAIN) || packet_ret == AVERROR_EOF) { break; }
        if (packet_ret < 0) { break; }
        av_packet_rescale_ts(m_packet, m_codec_ctx->time_base, m_format_ctx->streams[0]->time_base);
        m_packet->stream_index = m_format_ctx->streams[0]->index;
        auto write_ret = av_interleaved_write_frame(m_format_ctx, m_packet);
        av_packet_unref(m_packet);
        if (write_ret < 0) { break; }
      }
    }
  }
  std::unique_lock<SpinLock> lock(m_lock);
  auto ret = avcodec_send_frame(m_codec_ctx, nullptr);
  if (ret < 0) {
    std::cerr << "Error sending frame: " << avErr2String(ret) << std::endl;
    return;
  }
  while (m_codec_ctx) {
    auto flush_packet_ret = avcodec_receive_packet(m_codec_ctx, m_packet);
    if (flush_packet_ret == AVERROR(EAGAIN) ||
        flush_packet_ret == AVERROR_EOF) {
      break;
    } else if (flush_packet_ret < 0) {
      std::cerr << "Error receiving packet: " << avErr2String(flush_packet_ret)
                << std::endl;
      break;
    }
    av_packet_rescale_ts(m_packet, m_codec_ctx->time_base,
                         m_format_ctx->streams[0]->time_base);
    m_packet->stream_index = m_format_ctx->streams[0]->index;
    auto flush_write_ret = av_interleaved_write_frame(m_format_ctx, m_packet);
    if (flush_write_ret < 0) {
      std::cerr << "Error writing packet: " << avErr2String(flush_write_ret)
                << std::endl;
      break;
    }
    av_packet_unref(m_packet);
  }

  if (m_format_ctx) {
    auto trailer_ret = av_write_trailer(m_format_ctx);
    if (trailer_ret < 0) {
      std::cerr << "Error writing trailer: " << avErr2String(trailer_ret)
                << std::endl;
    }
  }
}

void AudioEncoder::initSwr() {
  auto swr_ctx = swr_alloc();
  if (!swr_ctx) {
    throw std::runtime_error("Failed to allocate resampler context");
  }
  AVChannelLayout in_ch_layout;
  av_channel_layout_default(&in_ch_layout, m_config.in_channels);
  av_opt_set_chlayout(swr_ctx, "in_chlayout", &in_ch_layout, 0);
  av_opt_set_int(swr_ctx, "in_sample_rate", m_config.in_sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_config.in_sample_format, 0);

  av_opt_set_chlayout(swr_ctx, "out_chlayout", &m_codec_ctx->ch_layout, 0);
  av_opt_set_int(swr_ctx, "out_sample_rate", m_codec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", m_codec_ctx->sample_fmt, 0);

  int ret = swr_init(swr_ctx);
  if (ret < 0) {
    swr_free(&swr_ctx);
    throw std::runtime_error("Failed to initialize resampler: " +
                             avErr2String(ret));
  }
  m_swr_ctx = swr_ctx;
  m_swr_in_data.resize(m_config.in_channels, nullptr);
  for (int ch = 0; ch < m_config.in_channels; ch++) {
    m_swr_in_data[ch] = nullptr;
  }
}

AVFrame *AudioEncoder::resample2Frame(uint8_t *data, int size) {
  if (!data || size <= 0) {
    return nullptr;
  }
  av_frame_unref(m_frame);
  int in_nb_samples = size /
                      av_get_bytes_per_sample(m_config.in_sample_format) /
                      m_config.in_channels;
  m_frame->format = m_codec_ctx->sample_fmt;
  m_frame->ch_layout = m_codec_ctx->ch_layout;
  m_frame->sample_rate = m_codec_ctx->sample_rate;

  int in_bytes_per_sample = av_get_bytes_per_sample(m_config.in_sample_format);
  int out_bytes_per_sample =
      av_get_bytes_per_sample(m_config.out_sample_format);
  if (!m_swr_ctx) {
    m_frame->nb_samples = in_nb_samples;
    int ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
      std::cerr << "Error allocating frame buffer: " << avErr2String(ret)
                << std::endl;
      return nullptr;
    }
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
      std::cerr << "Error making frame writable: " << avErr2String(ret)
                << std::endl;
      return nullptr;
    }
    if (av_sample_fmt_is_planar(m_config.out_sample_format)) {
      for (int ch = 0; ch < m_config.out_channels; ch++) {
        m_frame->linesize[ch] = in_nb_samples * out_bytes_per_sample;
        memcpy(m_frame->data[ch],
               data + ch * in_nb_samples * out_bytes_per_sample,
               in_nb_samples * out_bytes_per_sample);
      }
    } else {
      memcpy(m_frame->data[0], data,
             in_nb_samples * m_config.out_channels * out_bytes_per_sample);
      m_frame->linesize[0] =
          in_nb_samples * m_config.out_channels * out_bytes_per_sample;
    }
    return m_frame;
  }

  int out_samples = av_rescale_rnd(
      in_nb_samples + swr_get_delay(m_swr_ctx, m_config.in_sample_rate),
      m_config.out_sample_rate, m_config.in_sample_rate, AV_ROUND_UP);

  if (out_samples <= 0) {
    return nullptr;
  }

  m_frame->nb_samples = out_samples;
  int ret = av_frame_get_buffer(m_frame, 0);
  if (ret < 0) {
    std::cerr << "Error allocating frame buffer: " << avErr2String(ret)
              << std::endl;
    return nullptr;
  }

  ret = av_frame_make_writable(m_frame);
  if (ret < 0) {
    std::cerr << "Error making frame writable: " << avErr2String(ret)
              << std::endl;
    return nullptr;
  }

  if (av_sample_fmt_is_planar(m_config.in_sample_format)) {
    for (int ch = 0; ch < m_config.in_channels; ch++) {
      m_swr_in_data[ch] = data + ch * in_nb_samples * in_bytes_per_sample;
    }
  } else {
    m_swr_in_data[0] = data;
  }

  int cvr_nb_samples =
      swr_convert(m_swr_ctx, m_frame->data, m_frame->nb_samples,
                  m_swr_in_data.data(), in_nb_samples);

  if (cvr_nb_samples <= 0) {
    std::cerr << "swr_convert failed: " << cvr_nb_samples << std::endl;
    return nullptr;
  }

  m_frame->pts = m_next_pts;
  m_frame->nb_samples = cvr_nb_samples;

  m_next_pts +=
      av_rescale_q(m_frame->nb_samples, av_make_q(1, m_config.out_sample_rate),
                   m_codec_ctx->time_base);

  if (av_sample_fmt_is_planar(m_config.out_sample_format)) {
    for (int ch = 0; ch < m_config.out_channels; ch++) {
      m_frame->linesize[ch] = m_frame->nb_samples * out_bytes_per_sample;
    }
  } else {
    m_frame->linesize[0] =
        m_frame->nb_samples * m_config.out_channels * out_bytes_per_sample;
  }
  return m_frame;
}

AVFrame *AudioEncoder::flushSwr() {
  if (!m_swr_ctx) {
    return nullptr;
  }
  av_frame_unref(m_frame);
  int in_nb_samples = swr_get_delay(m_swr_ctx, m_config.in_sample_rate);
  int out_nb_samples = av_rescale_rnd(in_nb_samples, m_config.out_sample_rate,
                                      m_config.in_sample_rate, AV_ROUND_UP);
  int out_bytes_per_sample =
      av_get_bytes_per_sample(m_config.out_sample_format);

  if (out_nb_samples <= 0) {
    return nullptr;
  }
  m_frame->format = m_codec_ctx->sample_fmt;
  m_frame->ch_layout = m_codec_ctx->ch_layout;
  m_frame->sample_rate = m_codec_ctx->sample_rate;
  m_frame->nb_samples = out_nb_samples;

  int ret = av_frame_get_buffer(m_frame, 0);
  if (ret < 0) {
    std::cerr << "Error allocating frame buffer: " << avErr2String(ret)
              << std::endl;
    return nullptr;
  }
  ret = av_frame_make_writable(m_frame);
  if (ret < 0) {
    std::cerr << "Error making frame writable: " << avErr2String(ret)
              << std::endl;
    return nullptr;
  }

  int cvr_nb_samples =
      swr_convert(m_swr_ctx, m_frame->data, m_frame->nb_samples, nullptr, 0);
  if (cvr_nb_samples < 0) {
    std::cerr << "swr_convert failed: " << cvr_nb_samples << std::endl;
    return nullptr;
  }
  if (cvr_nb_samples == 0) {
    return nullptr;
  }
  m_frame->pts = m_next_pts;
  m_frame->nb_samples = cvr_nb_samples;
  m_next_pts +=
      av_rescale_q(m_frame->nb_samples, av_make_q(1, m_codec_ctx->sample_rate),
                   m_codec_ctx->time_base);

  if (av_sample_fmt_is_planar(m_codec_ctx->sample_fmt)) {
    for (int ch = 0; ch < m_config.out_channels; ch++) {
      m_frame->linesize[ch] = m_frame->nb_samples * out_bytes_per_sample;
    }
  } else {
    m_frame->linesize[0] =
        m_frame->nb_samples * m_config.out_channels * out_bytes_per_sample;
  }
  return m_frame;
}
