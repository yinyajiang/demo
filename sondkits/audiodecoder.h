#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}



class AudioDecoder {
public:
    AudioDecoder(int target_sample_rate, int target_channels, AVSampleFormat target_sample_format);
    ~AudioDecoder();

    void open(const std::filesystem::path &in_fpath);
    void close();
    uint8_t * decode_next_frame_data(int *out_data_size);
    void seek(int64_t time_ms);

    AVFormatContext* format_context() const;
    AVCodecContext* codec_context() const;
    int audio_stream_index() const;
    double duration() const;
    bool is_end() const;

private:
  void init_swr();
  void resample_frame_append(AVFrame *frame, uint8_t **audio_data, int *data_size);
  uint8_t ** resample_frame(AVFrame *frame, int *out_buffer_size);
private:
    AVFormatContext* m_fmt_ctx;
    AVCodecContext *m_dec_ctx;
    SwrContext *m_swr_ctx;

    //buffer
    AVPacket *m_packet;
    AVFrame *m_frame;
    
    int m_in_astream_idx;
    int m_target_sample_rate;
    int m_target_channels;
    AVSampleFormat m_target_sample_format;
    bool m_is_end;
};
