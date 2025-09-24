#include "audioencoder.h"
#include <cstddef>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
}
#include "audioutils.h"
#include "common.h"
#include <iostream>
#include <mutex>

AudioEncoder::AudioEncoder() : m_codec_ctx(nullptr), m_format_ctx(nullptr), m_swr_ctx(nullptr), m_frame(nullptr), m_packet(nullptr), m_next_pts(0) {}

AudioEncoder::~AudioEncoder() { close(); }

void AudioEncoder::open(const std::filesystem::path &file_path,
                        AudioEncoderConfig config) {

  std::unique_lock<SpinLock> lock(m_lock);
  m_next_pts = 0;

  avformat_alloc_output_context2(&m_format_ctx, nullptr, nullptr, fs2u8(file_path).c_str());
  if (!m_format_ctx) {
    throw std::runtime_error("Failed to allocate output context");
  }

  if(config.out_codec_id == AV_CODEC_ID_NONE) {
        //根据后缀名获取格式
        std::string extension = file_path.extension().string();
        if (extension == ".wav") {
            config.out_codec_id = pcmAVSampleFormat2CodecId(config.in_sample_format);
        } else if(extension == ".mp3") {
            config.out_codec_id = AV_CODEC_ID_MP3;
        }else {
            config.out_codec_id = AV_CODEC_ID_AAC;
        }
    }
   auto codec = avcodec_find_encoder(config.out_codec_id);
   if(!codec) {
    throw std::runtime_error("Failed to find encoder");
   }

   AVStream *stream = avformat_new_stream(m_format_ctx, nullptr);
   if(!stream) {
    throw std::runtime_error("Failed to allocate stream");
   }

   m_codec_ctx = avcodec_alloc_context3(codec);
   if(!m_codec_ctx) {
    throw std::runtime_error("Failed to allocate codec context");
   }
   m_config = config;
   m_codec_ctx->sample_fmt = config.out_sample_format;
   m_codec_ctx->sample_rate = config.out_sample_rate;
   av_channel_layout_default(&m_codec_ctx->ch_layout, config.out_channels);
   m_codec_ctx->time_base = av_make_q(1, m_codec_ctx->sample_rate);
   m_codec_ctx->pkt_timebase = m_codec_ctx->time_base;

   initSwr();
   m_frame = av_frame_alloc();
   if(!m_frame) {
    throw std::runtime_error("Failed to allocate frame");
   }
   
   m_packet = av_packet_alloc();
   if(!m_packet) {
    throw std::runtime_error("Failed to allocate packet");
   }
   m_frame->format = m_config.out_sample_format;
   m_frame->ch_layout = m_codec_ctx->ch_layout;
   m_frame->sample_rate = m_config.out_sample_rate;
   m_next_pts = 0;

   auto ret = avcodec_open2(m_codec_ctx, codec, nullptr);
   if (ret < 0) {
    throw std::runtime_error("Failed to open codec: " + avErr2String(ret));
   }
   ret = avcodec_parameters_from_context(stream->codecpar, m_codec_ctx);
   if (ret < 0) {
    throw std::runtime_error("Failed to copy codec parameters: " + avErr2String(ret));
   }
   stream->time_base = m_codec_ctx->time_base;

   if (!(m_format_ctx->oformat->flags & AVFMT_NOFILE)) {
     ret = avio_open(&m_format_ctx->pb, fs2u8(file_path).c_str(), AVIO_FLAG_WRITE);
     if (ret < 0) {
       throw std::runtime_error("Failed to open output: " + avErr2String(ret));
     }
   }

   ret = avformat_write_header(m_format_ctx, nullptr);
   if (ret < 0) {
    throw std::runtime_error("Failed to write header: " + avErr2String(ret));
   }
}

void AudioEncoder::close() {
    std::unique_lock<SpinLock> lock(m_lock);
    if (m_format_ctx) {
        av_write_trailer(m_format_ctx);
        if (!(m_format_ctx->oformat->flags & AVFMT_NOFILE) && m_format_ctx->pb) {
            avio_closep(&m_format_ctx->pb);
        }
    }
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
    AVFrame *frame = resample2Frame(data, size);
    encodeData(frame);
}

void AudioEncoder::encodeData(AVFrame *frame) {
    if (!frame) {
        return;
    }
    std::unique_lock<SpinLock> lock(m_lock);
    if (!m_codec_ctx || !m_format_ctx || !m_packet || m_format_ctx->nb_streams == 0) {
        return;
    }

    avcodec_send_frame(m_codec_ctx, frame);
    while (m_codec_ctx) {
        auto ret = avcodec_receive_packet(m_codec_ctx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }else if (ret < 0) {
            std::cerr << "Error receiving packet: " << avErr2String(ret) << std::endl;
            break;
        }
        av_packet_rescale_ts(m_packet, m_codec_ctx->time_base,
                                m_format_ctx->streams[0]->time_base);
        m_packet->stream_index = m_format_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(m_format_ctx, m_packet);
        if (ret < 0) {
            std::cerr << "Error writing packet: " << avErr2String(ret) << std::endl;
            break;
        }
        av_packet_unref(m_packet);
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
    std::unique_lock<SpinLock> lock(m_lock);
    auto ret = avcodec_send_frame(m_codec_ctx, nullptr);
    if (ret < 0) {
      std::cerr << "Error sending frame: " << avErr2String(ret) << std::endl;
      return;
    }
    while (m_codec_ctx) {
      auto ret = avcodec_receive_packet(m_codec_ctx, m_packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        std::cerr << "Error receiving packet: " << avErr2String(ret) << std::endl;
        break;
      }
      av_packet_rescale_ts(m_packet, m_codec_ctx->time_base,
                           m_format_ctx->streams[0]->time_base);
      m_packet->stream_index = m_format_ctx->streams[0]->index;
      ret = av_interleaved_write_frame(m_format_ctx, m_packet);
      if (ret < 0) {
        std::cerr << "Error writing packet: " << avErr2String(ret) << std::endl;
        break;
      }
      av_packet_unref(m_packet);
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
      throw std::runtime_error("Failed to initialize resampler: " + avErr2String(ret));
    }
    m_swr_ctx = swr_ctx;
}

AVFrame *AudioEncoder::resample2Frame(uint8_t *data, int size) {
    if (!data || size <= 0) {
      return nullptr;
    }
    int in_sample_size = av_get_bytes_per_sample(m_config.in_sample_format);
    int in_nb_samples = size / (in_sample_size * m_config.in_channels);

    int out_samples = av_rescale_rnd(
        in_nb_samples + swr_get_delay(m_swr_ctx, m_config.in_sample_rate),
        m_config.out_sample_rate, m_config.in_sample_rate, AV_ROUND_UP);
  
    if (out_samples <= 0) {
      return nullptr;
    }
  
    av_frame_unref(m_frame);
    m_frame->format = m_codec_ctx->sample_fmt;
    m_frame->ch_layout = m_codec_ctx->ch_layout;
    m_frame->sample_rate = m_codec_ctx->sample_rate;
    m_frame->nb_samples = out_samples;
    int ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
      std::cerr << "Error allocating frame buffer: " << avErr2String(ret) << std::endl;
      return nullptr;
    }
    
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
      std::cerr << "Error making frame writable: " << avErr2String(ret) << std::endl;
      return nullptr;
    }
  
    const uint8_t **in_data = (const uint8_t**)av_calloc(m_config.in_channels, sizeof(uint8_t*));
    memset(in_data, 0, m_config.in_channels * sizeof(uint8_t*));
    if (av_sample_fmt_is_planar(m_config.in_sample_format)) {
        for (int ch = 0; ch < m_config.in_channels; ch++) {
            in_data[ch] = data + ch * in_nb_samples * in_sample_size;
        }
    } else {
        in_data[0] = data;
    }
    
    int out_nb_samples = swr_convert(m_swr_ctx, m_frame->data, out_samples, 
                                   in_data, in_nb_samples);
    
    av_free(in_data);
    
    if (out_nb_samples <= 0) {
      std::cerr << "swr_convert failed: " << out_nb_samples << std::endl;
      return nullptr;
    }
    
    // 设置最终的frame属性
    m_frame->pts = m_next_pts;
    m_frame->nb_samples = out_nb_samples;

    m_next_pts +=
        av_rescale_q(out_nb_samples, av_make_q(1, m_config.out_sample_rate),
                     m_codec_ctx->time_base);
    
    if (av_sample_fmt_is_planar(m_config.out_sample_format)) {
        int bytes_per_sample = av_get_bytes_per_sample(m_config.out_sample_format);
        for (int ch = 0; ch < m_config.out_channels; ch++) {
            m_frame->linesize[ch] = out_nb_samples * bytes_per_sample;
        }
    } else {
        int bytes_per_sample = av_get_bytes_per_sample(m_config.out_sample_format);
        m_frame->linesize[0] = out_nb_samples * m_config.out_channels * bytes_per_sample;
    }
    return m_frame;
}


AVFrame* AudioEncoder::flushSwr() {
    if (!m_swr_ctx) {
        return nullptr;
    }
    int in_nb_samples = swr_get_delay(m_swr_ctx, m_config.in_sample_rate);
    int out_samples = av_rescale_rnd(in_nb_samples, m_config.out_sample_rate, m_config.in_sample_rate, AV_ROUND_UP);
  
    if (out_samples <= 0) {
      return nullptr;
    }
    av_frame_unref(m_frame);
    m_frame->format = m_codec_ctx->sample_fmt;
    m_frame->ch_layout = m_codec_ctx->ch_layout;
    m_frame->sample_rate = m_codec_ctx->sample_rate;
    m_frame->nb_samples = out_samples;
    int ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
      std::cerr << "Error allocating frame buffer: " << avErr2String(ret) << std::endl;
      return nullptr;
    }
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
      std::cerr << "Error making frame writable: " << avErr2String(ret) << std::endl;
      return nullptr;
    }

    int out_nb_samples = swr_convert(m_swr_ctx, m_frame->data, out_samples, nullptr, 0);
    if (out_nb_samples < 0) {
      std::cerr << "swr_convert failed: " << out_nb_samples << std::endl;
      return nullptr;
    }
    if (out_nb_samples == 0) {
        return nullptr;
    }
    m_frame->pts = m_next_pts;
    m_frame->nb_samples = out_nb_samples;
    m_next_pts +=
        av_rescale_q(out_nb_samples, av_make_q(1, m_codec_ctx->sample_rate),
                     m_codec_ctx->time_base);
    
    if (av_sample_fmt_is_planar(m_codec_ctx->sample_fmt)) {
        int bytes_per_sample = av_get_bytes_per_sample(m_codec_ctx->sample_fmt);
        for (int ch = 0; ch < m_config.out_channels; ch++) {
            m_frame->linesize[ch] = out_nb_samples * bytes_per_sample;
        }
    } else {
        int bytes_per_sample = av_get_bytes_per_sample(m_codec_ctx->sample_fmt);
        m_frame->linesize[0] = out_nb_samples * m_config.out_channels * bytes_per_sample;
    }
    return m_frame;
}
