#pragma once

#include "decoder.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}
#include <future>
#include <semaphore>

class DecodeQueue {
public:
  DecodeQueue(std::shared_ptr<DecoderInterface> decoder,
              int max_frames_size = 350);
  ~DecodeQueue();

  void start();
  void stop();
  void clear();
  FrameData pop();
  int64_t readData(uint8_t *data, int64_t size);
  int64_t bytesAvailable();
  bool aborted();
  bool readEnded();

private:
  void push(FrameDataList &&items);
  bool isFull();
  bool isDecodeNotWorded();
  bool isEmpty();

  void decodeLoop();
  void stopLoop();

  void wakeup(bool include_decode);

  void resumeDecodableCallback();

private:
  FrameDataList m_frames;
  const int64_t m_max_frames_size;
  std::mutex m_mutex;
  std::condition_variable m_cv_read;
  std::condition_variable m_cv_decode;

  std::shared_ptr<DecoderInterface> m_decoder;

  std::thread m_decode_thread;
  std::binary_semaphore m_decode_semaphore;

  std::atomic<bool> m_decode_thread_stoped;
  std::atomic<bool> m_decoder_end;
  std::atomic<bool> m_abort;
  std::atomic<int64_t> m_front_pos;
  std::atomic<int64_t> m_datas_bytes_available;
};
