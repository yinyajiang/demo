#include "common.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <thread>
#include <filesystem>
#include <vector>
#include <cwchar>
#include <cstdlib>
#if _WIN32
#include <Windows.h>
#endif

std::string avErr2String(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return std::string(errbuf);
}

static int _WORKING_SAMPLE_RATE = 44100;
int WORKING_SAMPLE_RATE() { return _WORKING_SAMPLE_RATE; }
void SET_WORKING_SAMPLE_RATE(int sample_rate) { _WORKING_SAMPLE_RATE = sample_rate; }


void SpinLock::lock() {
  while (flag.test_and_set(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}
void SpinLock::unlock() { flag.clear(std::memory_order_release); }

bool SpinLock::try_lock() {
  return !flag.test_and_set(std::memory_order_acquire);
}