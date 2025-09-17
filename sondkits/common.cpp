#include "common.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
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