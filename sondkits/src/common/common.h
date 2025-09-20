#pragma once

#include <atomic>
#include <string>

// ffplay -f s16le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f s32le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f f32le -ar 44100 -ch_layout stereo decode.pcm

#define PRINT_CONSUME_TIME 1
#define USE_AUBIO_BPM 1
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLE_AV_FORMAT AV_SAMPLE_FMT_FLT
#define MAX_TEMPO 2.0f
#define MIN_TEMPO 0.1f

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
