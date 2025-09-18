#pragma once

#include "common.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class DecodeQueue {
public:
  DecodeQueue(std::shared_ptr<DecoderInterface> decoder, int max_frames_size = 350);
  ~DecodeQueue();

  void start();
  void stop();
  void clear();
  void restart();
  FrameData pop();
  int64_t readData(uint8_t *data, int64_t size);
  int64_t readDataUntil(uint8_t *data, int64_t size);
  int64_t bytesAvailable();
  bool aborted();
  bool canRead();

private:
  void push(FrameDataList &&items);
  bool is_full();
  bool is_loop_stopped();
  bool is_decode_stopped();
  bool is_empty();

  void decode_loop();
  void stop_loop();

private:
  FrameDataList m_frames;
  const int64_t m_max_frames_size;
  std::mutex m_mutex;
  std::condition_variable m_cv_read;
  std::condition_variable m_cv_decode;

  std::shared_ptr<DecoderInterface> m_decoder;

  std::thread m_decode_thread;

  std::atomic<bool> m_decode_loop_stopped;
  std::atomic<bool> m_abort;
  std::atomic<int64_t> m_front_pos;
  std::atomic<int64_t> m_datas_byte_size;
};
