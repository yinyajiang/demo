#pragma once
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

void av_bufferref_deleter(AVBufferRef *bufferref);

void av_frame_deleter(AVFrame *frame);

void av_packet_deleter(AVPacket* packet);

void swr_context_deleter(SwrContext* swr_ctx);


std::string av_err2string(int errnum);

void av_append_buffer(uint8_t **dst, int *dst_size, uint8_t *src, int src_size);
