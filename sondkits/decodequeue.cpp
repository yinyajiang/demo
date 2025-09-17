#include "decodequeue.h"
#include <iostream>
#include <mutex>

DecodeQueue::DecodeQueue(std::shared_ptr<DecoderInterface> decoder,
                         int max_buffer_size)
    : m_decoder(decoder), m_max_datas_size(max_buffer_size),
      m_decode_loop_stopped(false), m_abort(false), m_front_pos(0),
      m_datas_byte_size(0) {}

DecodeQueue::~DecodeQueue() { stop(); }

void DecodeQueue::start() {
  m_abort.store(false);
  m_decode_loop_stopped.store(false);
  m_decode_thread = std::thread([this]() { decode_loop(); });
}

void DecodeQueue::restart() {
  stop();
  clear();
  start();
}

void DecodeQueue::clear() {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_datas.clear();
  m_front_pos.store(0);
  m_datas_byte_size.store(0);
}

void DecodeQueue::stop() {
  m_abort.store(true);
  stop_loop();
  if (m_decode_thread.joinable()) {
    m_decode_thread.join();
  }
}

bool DecodeQueue::aborted() { return m_abort.load(); }

bool DecodeQueue::read_ended() {
  return aborted() || (is_empty() && is_decode_stopped());
}

int64_t DecodeQueue::read_data_until(uint8_t *buffer, int64_t buffer_size) {
  int64_t readed = 0;
  while (!read_ended()) {
    readed += read_data(buffer + readed, buffer_size - readed);
    if (readed >= buffer_size) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return readed;
}

int64_t DecodeQueue::read_data(uint8_t *buffer, int64_t buffer_size) {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (is_empty()) {
    if (aborted()) {
      return 0;
    }
    m_cv_read.wait(lock, [this]() -> bool {
      return aborted() || !is_empty() || is_decode_stopped();
    });
  }

  int64_t readed = 0;
  bool first = true;

  int count = 0;
  for (auto it = m_datas.begin();
       it != m_datas.end() && readed < buffer_size;) {
    auto &data = *it;
    int pos = 0;
    int size = data.size;
    if (first) {
      pos = m_front_pos.load();
      size -= pos;
      first = false;
    }

    if (buffer_size - readed >= size) {
      memcpy(buffer + readed, data.data + pos, size);
      readed += size;

      m_decoder->free_data(data.data);
      it = m_datas.erase(it);
      m_front_pos.store(0);
      m_datas_byte_size.fetch_sub(size);
    } else {
      auto copyed = buffer_size - readed;
      memcpy(buffer + readed, data.data + pos, copyed);
      readed += copyed;
      m_front_pos.fetch_add(copyed);
      ++it;
    }
  }

  m_cv_decode.notify_one();
  return readed;
}

int64_t DecodeQueue::bytes_available() {
  return m_datas_byte_size.load() - m_front_pos.load();
}

FrameData DecodeQueue::pop() {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (is_empty()) {
    if (aborted()) {
      return FrameData();
    }
    m_cv_read.wait(lock, [this]() -> bool {
      return aborted() || !is_empty() || is_decode_stopped();
    });
  }
  auto data = m_datas.front();
  m_datas.pop_front();

  m_front_pos.store(0);
  m_datas_byte_size.fetch_sub(data.size);

  m_cv_decode.notify_one();
  return std::move(data);
}

void DecodeQueue::push(FrameDataList &&items) {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (is_full()) {
    if (aborted()) {
      return;
    }
    m_cv_decode.wait(lock);
  }
  for (auto &data : items) {
    m_datas_byte_size.fetch_add(data.size);
    m_datas.push_back(std::move(data));
  }
  m_cv_read.notify_one();
}

void DecodeQueue::decode_loop() {
  while (!aborted()) {
    auto data = m_decoder->decode_next_frame_data();
    if (aborted()) {
      break;
    }

    if (data.empty()) {
      if (m_decoder->is_end()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(0));
      continue;
    }
    push(std::move(data));
  }
  stop_loop();
}

void DecodeQueue::stop_loop() {
  m_decode_loop_stopped.store(true);
  m_cv_read.notify_all();
  m_cv_decode.notify_all();
}

bool DecodeQueue::is_empty() { return m_datas.empty(); }

bool DecodeQueue::is_full() { return m_datas.size() >= m_max_datas_size; }

bool DecodeQueue::is_decode_stopped() { return m_decode_loop_stopped.load(); }