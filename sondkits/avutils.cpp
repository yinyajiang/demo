#include "avutils.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}


// Deleter for AVBufferRef unique_ptr
void av_bufferref_deleter(AVBufferRef* bufferref) {
    if (bufferref != nullptr) {
        av_buffer_unref(&bufferref);
    }
}

// Deleter for AVFrame unique_ptr
void av_frame_deleter(AVFrame* frame) {
    if (frame != nullptr) {
        av_frame_free(&frame);
        frame = nullptr;
    }
}

// Deleter for AVPacket unique_ptr
void av_packet_deleter(AVPacket* packet) {
    if (packet != nullptr) {
        av_packet_unref(packet);
        av_packet_free(&packet);
        packet = nullptr;
    }
}

// Deleter for SwrContext unique_ptr
void swr_context_deleter(SwrContext* swr_ctx) {
    if (swr_ctx != nullptr) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
}


std::string av_err2string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

void av_append_buffer(uint8_t **dst, int *dst_size, uint8_t *src, int src_size) {
    if (!dst || !dst_size || !src || src_size <= 0) {
        return;
    }
    
    uint8_t *new_ptr = (uint8_t*)av_realloc(*dst, *dst_size + src_size);
    if (!new_ptr) {
        // Memory allocation failed, keep original buffer unchanged
        return;
    }
    *dst = new_ptr;
    memcpy(*dst + *dst_size, src, src_size);
    *dst_size += src_size;
}