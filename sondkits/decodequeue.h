#pragma once

#include <list>
#include <mutex>
#include <condition_variable>
#include "common.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

class DecodeQueue {
  public:
    DecodeQueue(std::shared_ptr<DecoderInterface> decoder, int max_buffer_size = 100);
    ~DecodeQueue();

    void start();
    void stop();
    void clear();
    void restart();
    FrameData pop();
    int64_t read_data(uint8_t *data, int size);
    int64_t bytes_available();
    bool is_abort();
    bool is_decode_stopped();
    bool is_empty();
 
  private:
    void push(FrameDataList &&items);

    bool is_full();
    bool is_loop_stopped();


    void decode_loop();
    void stop_loop();

  private:
    FrameDataList m_datas;
    std::mutex m_mutex;
    std::condition_variable m_cv_read;
    std::condition_variable m_cv_decode;
    int64_t m_max_buffer_size;

    std::shared_ptr<DecoderInterface> m_decoder;
    std::atomic<bool> m_loop_stopped;
    std::atomic<bool> m_abort;
    std::thread m_thread;

    std::atomic<int64_t> m_front_pos;
    std::atomic<int64_t> m_datas_byte_size;
    
};
