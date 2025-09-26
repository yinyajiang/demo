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
}
#include "audioutils.h"
#include "common.h"
#include <iostream>
#include <mutex>

AudioEncoder::AudioEncoder()
    : m_codec_ctx(nullptr), m_format_ctx(nullptr), m_swr_ctx(nullptr),
      m_frame(nullptr), m_packet(nullptr), m_next_pts(0), m_fifo(nullptr) {}

AudioEncoder::~AudioEncoder() { close(); }

void AudioEncoder::open(const std::filesystem::path &file_path, int in_sample_rate, int in_channels, AVSampleFormat in_sample_format) {
    assert(av_sample_fmt_is_planar(in_sample_format) == 0);
    std::unique_lock<SpinLock> lock(m_lock);
    m_next_pts = 0;

    int ret = avformat_alloc_output_context2(&m_format_ctx, nullptr, nullptr,
                                            fs2u8(file_path).c_str());
    if (ret < 0) {
        throw std::runtime_error("[avformat_alloc_output_context2]: " +
                                avErr2String(ret));
    }
    m_in_sample_rate = in_sample_rate;
    m_in_channels = in_channels;
    m_in_sample_format = in_sample_format;

    AVCodecID codec_id;
    CodecOptions opts;
    opts.sample_fmt = in_sample_format;
    opts.sample_rate = in_sample_rate;
    av_channel_layout_default(&opts.channel_layout, in_channels);
    opts.bit_rate = 0;
    std::string extension = toLower(file_path.extension().string());
    if (extension == ".wav") {
        codec_id = pcmSampleFmt2CodecId(in_sample_format);
    } else if (extension == ".mp3") {
      codec_id = AV_CODEC_ID_MP3;
      opts.bit_rate = ENCODER_MP3_BIT_RATE;
    } else {
        throw std::runtime_error("only support wav and mp3");
    }
    opts = getPrefferCodecOptions(codec_id, opts);

    auto codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        throw std::runtime_error("[avcodec_find_encoder]: " +
                                std::to_string(codec_id));
    }

    AVStream *stream = avformat_new_stream(m_format_ctx, nullptr);
    if (!stream) {
        throw std::runtime_error("[avformat_new_stream]");
    }

    m_codec_ctx = avcodec_alloc_context3(codec);
    if (!m_codec_ctx) {
        throw std::runtime_error("Failed to allocate codec context");
    }

    m_codec_ctx->sample_fmt = opts.sample_fmt;
    m_codec_ctx->sample_rate = opts.sample_rate;
    av_channel_layout_default(&m_codec_ctx->ch_layout, opts.channel_layout.nb_channels);
    m_codec_ctx->time_base = av_make_q(1, m_codec_ctx->sample_rate);
    m_codec_ctx->pkt_timebase = m_codec_ctx->time_base;
    if (codec_id == AV_CODEC_ID_MP3) {
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
    m_next_pts = 0;

    ret = avcodec_open2(m_codec_ctx, codec, nullptr);
    if (ret < 0) {
        throw std::runtime_error("[avcodec_open2]: " +
                                avErr2String(ret));
    }
    // Ensure encoder works with interleaved (packed) samples only
    if (av_sample_fmt_is_planar((AVSampleFormat)m_codec_ctx->sample_fmt) != 0) {
        throw std::runtime_error("AudioEncoder only supports interleaved (packed) sample format");
    }
    ret = avcodec_parameters_from_context(stream->codecpar, m_codec_ctx);
    if (ret < 0) {
        throw std::runtime_error("[avcodec_parameters_from_context]: " +
                                avErr2String(ret));
    }
    stream->time_base = m_codec_ctx->time_base;

    if (!(m_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (!std::filesystem::exists(file_path.parent_path())) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        ret =
            avio_open(&m_format_ctx->pb, fs2u8(file_path).c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            throw std::runtime_error("[avio_open]: " + avErr2String(ret));
        }
    }

    ret = avformat_write_header(m_format_ctx, nullptr);
    if (ret < 0) {
        throw std::runtime_error("[avformat_write_header]: " +
                                avErr2String(ret));
    }

    if (m_in_channels == m_codec_ctx->ch_layout.nb_channels &&
        m_in_sample_rate == m_codec_ctx->sample_rate &&
        m_in_sample_format == m_codec_ctx->sample_fmt) {
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

void AudioEncoder::intoFifoEncodeData(uint8_t **sample_data, int nb_samples) {
  // Ensure frame.nb_samples satisfies encoder's required frame_size (e.g., MP3 1152)
  int required =
      (m_codec_ctx->frame_size > 0) ? m_codec_ctx->frame_size : nb_samples;
  
  if (!m_fifo) {
    int fifo_size = (required > 0) ? required : 1024;
    m_fifo = av_audio_fifo_alloc((AVSampleFormat)m_codec_ctx->sample_fmt,
                                 m_codec_ctx->ch_layout.nb_channels,
                                 fifo_size);
    if (!m_fifo) {
      std::cerr << "[av_audio_fifo_alloc] failed" << std::endl;
      return;
    }
  }
  
  if(required == 0){
     required = av_audio_fifo_size(m_fifo);
  }

  if (nb_samples > 0) {
    // write incoming samples into FIFO， auto realloc if needed
    int writed = av_audio_fifo_write(m_fifo, (void **)sample_data, nb_samples);

    if (writed < nb_samples) {
        std::cerr << "fifo write failed" << std::endl;
        return;
    }
  }

  while (required > 0 && av_audio_fifo_size(m_fifo) >= required) {
    av_frame_unref(m_frame);
    m_frame->format = m_codec_ctx->sample_fmt;
    m_frame->ch_layout = m_codec_ctx->ch_layout;
    m_frame->sample_rate = m_codec_ctx->sample_rate;
    m_frame->nb_samples = required;
    int ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
      std::cerr << "[av_frame_get_buffer] failed: " << avErr2String(ret) << std::endl;
      break;
    }
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
      std::cerr << "[av_frame_make_writable] failed: " << avErr2String(ret) << std::endl;
      break;
    }
    int read_samples = av_audio_fifo_read(m_fifo, (void **)m_frame->data, required);
    if (read_samples < required) {
      std::cerr << "[av_audio_fifo_read] failed: expected " << required << " got " << read_samples << std::endl;
      break;
    }
    encodeFrame(m_frame);
  }

  if(nb_samples == 0){
    int remain = av_audio_fifo_size(m_fifo);
    if (remain <= 0) {
      return;
    }
    required = (m_codec_ctx->frame_size > 0) ? m_codec_ctx->frame_size : remain;

    av_frame_unref(m_frame);
    m_frame->format = m_codec_ctx->sample_fmt;
    m_frame->ch_layout = m_codec_ctx->ch_layout;
    m_frame->sample_rate = m_codec_ctx->sample_rate;
    m_frame->nb_samples = required;
    int ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
        std::cerr << "[av_frame_get_buffer] failed: " << avErr2String(ret) << std::endl;
        return;
    }
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
        std::cerr << "[av_frame_make_writable] failed: " << avErr2String(ret) << std::endl;
        return;
    }
    int read_samples = av_audio_fifo_read(m_fifo, (void **)m_frame->data, remain);
    if (read_samples < remain) {
        std::cerr << "[av_audio_fifo_read] failed: expected " << remain << " got " << read_samples << std::endl;
        return;
    }
    if (remain < required) {
      int bytes_per_frame_sample = av_get_bytes_per_sample(m_codec_ctx->sample_fmt) * m_codec_ctx->ch_layout.nb_channels;
      memset(m_frame->data[0] + remain * bytes_per_frame_sample, 0, (required - remain) * bytes_per_frame_sample);
    }
    encodeFrame(m_frame);
  }
}

bool AudioEncoder::encodeFrame(AVFrame *frame) {
    if (frame) {
        frame->pts = m_next_pts;
        m_next_pts += av_rescale_q(frame->nb_samples, av_make_q(1, m_codec_ctx->sample_rate), m_codec_ctx->time_base);
    }

    auto ret = avcodec_send_frame(m_codec_ctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      std::cerr << "[avcodec_send_frame] failed: " << avErr2String(ret) << std::endl;
      return false;
    }
    bool bSuccess = false;
    while (m_codec_ctx) {
      av_packet_unref(m_packet);
      ret = avcodec_receive_packet(m_codec_ctx, m_packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        std::cerr << "[avcodec_receive_packet]: " << avErr2String(ret)
                  << std::endl;
        break;
      }
      av_packet_rescale_ts(m_packet, m_codec_ctx->time_base,
                           m_format_ctx->streams[0]->time_base);
      m_packet->stream_index = m_format_ctx->streams[0]->index;
      ret = av_interleaved_write_frame(m_format_ctx, m_packet);
      if (ret < 0) {
        std::cerr << "[av_interleaved_write_frame] failed: " << avErr2String(ret)
                  << std::endl;
        break;
      }
      bSuccess = true;
    }
    return bSuccess;
}

void AudioEncoder::encodeData(uint8_t *data, int size) {
  if (!data || size <= 0) {
    return;
  }
  std::unique_lock<SpinLock> lock(m_lock);
  if (!m_codec_ctx || !m_format_ctx) {
    return;
  }

  uint8_t *sample_data[10] = {0};
  int nb_samples = resample(data, size, sample_data);
  if (nb_samples > 0) {
    intoFifoEncodeData(sample_data, nb_samples);
    av_freep(&sample_data[0]);
  }
}

bool AudioEncoder::flush() {
    std::unique_lock<SpinLock> lock(m_lock);
    if (!m_codec_ctx || !m_format_ctx) {
        return false;
    }
    uint8_t *sample_data[10] = {0};
    int nb_samples = flushSwr(sample_data);
    if (nb_samples > 0) {
        intoFifoEncodeData(sample_data, nb_samples);
        av_freep(&sample_data[0]);
    }
    intoFifoEncodeData(nullptr, 0);
    encodeFrame(nullptr);
    
    auto ret = av_write_trailer(m_format_ctx);
    if (ret < 0) {
        std::cerr << "Error writing trailer: " << avErr2String(ret)
                << std::endl;
    }
    if (m_format_ctx && m_format_ctx->pb) {
        avio_close(m_format_ctx->pb);
        m_format_ctx->pb = nullptr;
    }
    return ret == 0;
}

void AudioEncoder::initSwr() {
  auto swr_ctx = swr_alloc();
  if (!swr_ctx) {
    throw std::runtime_error("Failed to allocate resampler context");
  }
  AVChannelLayout in_ch_layout;
  av_channel_layout_default(&in_ch_layout, m_in_channels);
  av_opt_set_chlayout(swr_ctx, "in_chlayout", &in_ch_layout, 0);
  av_opt_set_int(swr_ctx, "in_sample_rate", m_in_sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_in_sample_format, 0);

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
}

int AudioEncoder::resample(uint8_t *data, int size, uint8_t **out_sample_buff) {
  out_sample_buff[0] = nullptr;
  if (!data || size <= 0 || !out_sample_buff) {
    return 0;
  }
  int in_nb_samples = size /
                      av_get_bytes_per_sample(m_in_sample_format) /
                      m_in_channels;
  if (!m_swr_ctx) {
    uint8_t* pdata = (uint8_t*)av_malloc(size);
    if (!pdata) {
      std::cerr << "Failed to allocate memory for audio data" << std::endl;
      return 0;
    }
    memcpy(pdata, data, size);
    out_sample_buff[0] = pdata;
    return in_nb_samples;
  }

  int out_nb_samples = av_rescale_rnd(
      in_nb_samples + swr_get_delay(m_swr_ctx, m_in_sample_rate),
      m_codec_ctx->sample_rate, m_in_sample_rate, AV_ROUND_UP);

  if (out_nb_samples <= 0) {
    return 0;
  }

  uint8_t **audio_data = nullptr;
  int linesize;
  int ret = av_samples_alloc_array_and_samples(&audio_data, &linesize,
                                               m_codec_ctx->ch_layout.nb_channels, out_nb_samples,
                                               m_codec_ctx->sample_fmt, 0);
  if (ret < 0) {
    std::cerr << "Error allocating audio buffer: " << avErr2String(ret)
              << std::endl;
    return 0;
  }
  uint8_t *in_data[10] = {0};
  in_data[0] = data;
  out_nb_samples = swr_convert(m_swr_ctx, audio_data, out_nb_samples,
                        const_cast<const uint8_t **>(in_data),
                        in_nb_samples);
  if (out_nb_samples <= 0) {
    std::cerr << "Error converting swr" << std::endl;
    if (audio_data) {
      av_freep(&audio_data[0]);
      av_freep(&audio_data);
    }
    return 0;
  }
  out_sample_buff[0] = audio_data[0];
  av_freep(&audio_data);
  return out_nb_samples;
}

int AudioEncoder::flushSwr(uint8_t **out_sample_buff) {
    out_sample_buff[0] = nullptr;
    if (!m_swr_ctx) {
        return 0;
    }
    int in_nb_samples = swr_get_delay(m_swr_ctx, m_in_sample_rate);
    int out_nb_samples = av_rescale_rnd(in_nb_samples, m_codec_ctx->sample_rate,
                                        m_in_sample_rate, AV_ROUND_UP);
    if (out_nb_samples <= 0) {
        return 0;
    }

    uint8_t **audio_data = nullptr;
    int linesize;
    int ret = av_samples_alloc_array_and_samples(&audio_data, &linesize,
                                                m_codec_ctx->ch_layout.nb_channels, out_nb_samples,
                                                m_codec_ctx->sample_fmt, 0);
    if (ret < 0) {
        std::cerr << "Error allocating audio buffer: " << avErr2String(ret)
                    << std::endl;
        return 0;
    }

    out_nb_samples = swr_convert(m_swr_ctx, audio_data, out_nb_samples,
                    nullptr, 0);
    if (out_nb_samples <= 0) {
        std::cerr << "Error converting swr" << std::endl;
        if (audio_data) {
            av_freep(&audio_data[0]);
            av_freep(&audio_data);
        }
        return 0;
    }
    out_sample_buff[0] = audio_data[0];
    av_freep(&audio_data);
    return out_nb_samples;
}


