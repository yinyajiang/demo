#pragma once
#include <list>
#include <string>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}
#include <atomic>

// ffplay -f s16le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f s32le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f f32le -ar 44100 -ch_layout stereo decode.pcm

#define PRINT_READ_CONSUME_TIME 0
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLE_AV_FORMAT AV_SAMPLE_FMT_FLT
#define MAX_TEMPO 2.0f
#define MIN_TEMPO 0.1f

struct FrameData {
  uint8_t *data;
  int size;
};
using FrameDataList = std::list<FrameData>;

class DecoderInterface {
public:
  virtual ~DecoderInterface() = default;
  virtual FrameDataList decodeNextFrameData() = 0;
  virtual bool isEnd() const = 0;
  virtual void freeData(uint8_t **data) = 0;
};

class SpinLock {
private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
  SpinLock() = default;
  SpinLock(const SpinLock &) = delete;
  SpinLock &operator=(const SpinLock) = delete;
  void lock();
  void unlock();
};

std::string avErr2String(int errnum);
