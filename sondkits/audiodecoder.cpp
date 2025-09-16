#include "audiodecoder.h"
#include <iostream>
#include <stdexcept>
#include "avutils.h"
#include  <cassert>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}


AudioDecoder::AudioDecoder(int target_sample_rate, int target_channels,
                           AVSampleFormat target_sample_format)
    : m_fmt_ctx(nullptr), m_dec_ctx(nullptr), m_in_astream_idx(-1),
      m_target_sample_rate(target_sample_rate),
      m_target_channels(target_channels),
      m_target_sample_format(target_sample_format), m_is_end(false),
      m_packet(nullptr), m_frame(nullptr) {
        assert(av_sample_fmt_is_planar(target_sample_format) == 0);
      }

AudioDecoder::~AudioDecoder() {
    close();
}


void AudioDecoder::open(const std::filesystem::path& in_fpath) {
    if (avformat_open_input(&m_fmt_ctx, in_fpath.u8string().c_str(), nullptr, nullptr) < 0) {
       throw std::runtime_error("Could not open input file");
    }

    if (avformat_find_stream_info(m_fmt_ctx, nullptr) < 0) {
        throw std::runtime_error("Failed to retrieve input stream information");
    }

    int stream_index = av_find_best_stream(m_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (stream_index < 0) {
        throw std::runtime_error("Could not find audio stream in the input file");
    }

    AVStream* audio_stream = m_fmt_ctx->streams[stream_index];

    // Find the decoder for the audio stream
    const AVCodec* decoder = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!decoder) {
        throw std::runtime_error("Failed to find decoder for codec ID: " + std::to_string(audio_stream->codecpar->codec_id));
    }

    // Allocate the decoder context
    m_dec_ctx = avcodec_alloc_context3(decoder);
    if (!m_dec_ctx) {
        throw std::runtime_error("Failed to allocate the decoder context");
    }

    if (avcodec_parameters_to_context(m_dec_ctx, audio_stream->codecpar) < 0) {
        throw std::runtime_error("Failed to copy decoder parameters to input decoder context");
    }

    // Set the time base
    m_dec_ctx->time_base = audio_stream->time_base;
    m_dec_ctx->pkt_timebase = audio_stream->time_base;


    // Open the decoder
    int ret = avcodec_open2(m_dec_ctx, decoder, nullptr);
    if (ret < 0) {
        throw std::runtime_error("Failed to open decoder for stream #" + std::to_string(stream_index) + ": " + av_err2string(ret));
    }

    m_in_astream_idx = stream_index;
    m_is_end = false;
    if (m_packet == nullptr) {
        m_packet = av_packet_alloc();
    }
    if (m_frame == nullptr) {
        m_frame = av_frame_alloc();
    }

    init_swr();
}

AVFormatContext* AudioDecoder::format_context() const {
    return m_fmt_ctx;
}

AVCodecContext* AudioDecoder::codec_context() const { return m_dec_ctx; }

int AudioDecoder::audio_stream_index() const { return m_in_astream_idx; }

double AudioDecoder::duration() const { return double(m_fmt_ctx->duration) / AV_TIME_BASE; }

bool AudioDecoder::is_end() const { return m_is_end; }

void AudioDecoder::close() {
    if (m_dec_ctx) {
        avcodec_free_context(&m_dec_ctx);
        m_dec_ctx = nullptr;
    }
    if (m_fmt_ctx) {
        avformat_close_input(&m_fmt_ctx);
        m_fmt_ctx = nullptr;
    }
    if (m_swr_ctx) {
        swr_free(&m_swr_ctx);
        m_swr_ctx = nullptr;
    }
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
}

void AudioDecoder::init_swr() {
    auto swr_ctx = swr_alloc();
    if (!swr_ctx) {
        throw std::runtime_error("Failed to allocate resampler context");
    }
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &m_dec_ctx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", m_dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_dec_ctx->sample_fmt, 0);

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, m_target_channels);
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", m_target_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", m_target_sample_format, 0);

    int ret = swr_init(swr_ctx);
    if (ret < 0) {
        swr_free(&swr_ctx);
        throw std::runtime_error("Failed to initialize resampler: " + av_err2string(ret));
    }
    m_swr_ctx = swr_ctx;
}

uint8_t * AudioDecoder::decode_next_frame_data(int *out_data_size) {
  if (!out_data_size) {
    return nullptr;
  }
  
  *out_data_size = 0;
  
  if (!m_fmt_ctx || !m_dec_ctx || !m_packet || !m_frame) {
    return nullptr;
  }
  
  uint8_t *frame_data = nullptr;
  int frame_data_size = 0;
  
  while (true) {
    int ret = av_read_frame(m_fmt_ctx, m_packet);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        m_is_end = true;
        // 刷新解码器，获取剩余帧
        if (avcodec_send_packet(m_dec_ctx, nullptr) >= 0) {
          while (avcodec_receive_frame(m_dec_ctx, m_frame) == 0) {
            if (m_frame && m_frame->data[0] && m_frame->nb_samples > 0) {
              resample_frame_append(m_frame, &frame_data, &frame_data_size);
            }
          }
        }
      } else {
        std::cerr << "Error reading packet: " << av_err2string(ret) << std::endl;
      }
      break;
    }
    
    // 只处理音频流的包
    if (m_packet->stream_index != m_in_astream_idx) {
      av_packet_unref(m_packet);
      continue;
    }

    ret = avcodec_send_packet(m_dec_ctx, m_packet);
    av_packet_unref(m_packet); // 及时释放包
    
    if (ret < 0) {
      if (ret != AVERROR(EAGAIN)) {
        std::cerr << "Error sending packet: " << av_err2string(ret) << std::endl;
      }
      continue;
    }
    
    // 接收解码帧
    while ((ret = avcodec_receive_frame(m_dec_ctx, m_frame)) == 0) {
      if (m_frame && m_frame->data[0] && m_frame->nb_samples > 0) {
        resample_frame_append(m_frame, &frame_data, &frame_data_size);
      }
    }
    
    if (frame_data_size > 0) {
      break;
    }
  }
  
  *out_data_size = frame_data_size;
  return frame_data;
}


void AudioDecoder::resample_frame_append(AVFrame *frame, uint8_t **audio_data, int *data_size) {
    if (!frame || !audio_data || !data_size) {
        return;
    }
    
    int pdata_size = 0;
    auto pdata = resample_frame(frame, &pdata_size);
    
    if (pdata && pdata[0] && pdata_size > 0) {
        av_append_buffer(audio_data, data_size, pdata[0], pdata_size);
        av_freep(&pdata[0]);
    }
    
    if (pdata) {
        av_freep(&pdata);
    }
}


uint8_t **AudioDecoder::resample_frame(AVFrame *frame, int *out_buffer_size) {
    if (!frame || !out_buffer_size || !m_swr_ctx) {
        if (out_buffer_size) *out_buffer_size = 0;
        return nullptr;
    }
    
    *out_buffer_size = 0;
    
    if (frame->nb_samples <= 0) {
        return nullptr;
    }
    
    int out_samples = av_rescale_rnd(frame->nb_samples + swr_get_delay(m_swr_ctx, frame->sample_rate), 
                                     m_target_sample_rate, frame->sample_rate, AV_ROUND_UP);
    
    if (out_samples <= 0) {
        return nullptr;
    }
    
    uint8_t **audio_data = nullptr;
    int linesize;
    
    int ret = av_samples_alloc_array_and_samples(&audio_data, &linesize, m_target_channels, 
                                                 out_samples, m_target_sample_format, 0);
    if (ret < 0) {
        std::cerr << "Error allocating audio buffer: " << av_err2string(ret) << std::endl;
        return nullptr;
    }
    
    int num = swr_convert(m_swr_ctx, audio_data, out_samples, 
                         const_cast<const uint8_t**>(frame->data), frame->nb_samples);
    if (num < 0) {
        std::cerr << "Error converting frame: " << av_err2string(num) << std::endl;
        if (audio_data) {
            av_freep(&audio_data[0]);
            av_freep(&audio_data);
        }
        return nullptr;
    }
    
    *out_buffer_size = av_samples_get_buffer_size(&linesize, m_target_channels, num, m_target_sample_format, 0);
    return audio_data;
}

void AudioDecoder::seek(int64_t time_ms) {
    if (!m_fmt_ctx) {
        return;
    }
    if (time_ms <= 0) {
      av_seek_frame(m_fmt_ctx, m_in_astream_idx, 0, AVSEEK_FLAG_BACKWARD);
    } else {
      av_seek_frame(m_fmt_ctx, m_in_astream_idx, time_ms * AV_TIME_BASE / 1000, AVSEEK_FLAG_BACKWARD);
    }
    m_is_end = false;
    avcodec_flush_buffers(m_dec_ctx);
}
