#pragma once

#include <atomic>
#include <string>
#include <filesystem>

// ffplay -f s16le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f s32le -ar 44100 -ch_layout stereo decode.pcm
// ffplay -f f32le -ar 44100 -ch_layout stereo decode.pcm

#define PRINT_CONSUME_TIME 0
#define WORKING_CHANNELS 2
#define WORKING_SAMPLE_AV_FORMAT AV_SAMPLE_FMT_FLT
#define MAX_TEMPO 2.0f
#define MIN_TEMPO 0.1f
#define PLAYER_PROGRESS_TIMER_INTERVAL_MS 500



int WORKING_SAMPLE_RATE();
void SET_WORKING_SAMPLE_RATE(int sample_rate);

class SpinLock {
private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
  SpinLock() = default;
  SpinLock(const SpinLock &) = delete;
  SpinLock &operator=(const SpinLock&) = delete;
  void lock();
  void unlock();
  bool try_lock();
};

std::string avErr2String(int errnum);
std::string fs2u8(const std::filesystem::path& path);
