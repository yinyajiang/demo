#include "common.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <thread>


std::string avErr2String(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

void  SpinLock::lock(){ 
    while (flag.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}   
void SpinLock::unlock(){ 
    flag.clear(std::memory_order_release);
}   