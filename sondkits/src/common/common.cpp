#include "common.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <thread>
#include <filesystem>
#if _WIN32
#include <Windows.h>
#include <cwchar>
#endif

std::string avErr2String(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return std::string(errbuf);
}

std::string fs2u8(const std::filesystem::path& path) {
  #if _WIN32
    std::wstring wide_path = path.wstring();
    int buffer_size =
        WideCharToMultiByte(CP_UTF8, 0, wide_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (buffer_size == 0) {
        return std::string();
    }
    std::vector<char> buffer(buffer_size);
    WideCharToMultiByte(
        CP_UTF8, 0, wide_path.c_str(), -1, buffer.data(), buffer_size, nullptr, nullptr
    );
    return std::string(buffer.data());
  #else
    return path.string();
  #endif
  }

void SpinLock::lock() {
  while (flag.test_and_set(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}
void SpinLock::unlock() { flag.clear(std::memory_order_release); }

bool SpinLock::try_lock() {
  return !flag.test_and_set(std::memory_order_acquire);
}