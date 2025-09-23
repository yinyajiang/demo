
#include "decodequeue.h"
#include "decoder.h"
#include <mutex>

DecodeQueue::DecodeQueue(std::shared_ptr<DecoderInterface> decoder,
                         int max_frames_size)
    : m_decoder(decoder), m_max_frames_size(max_frames_size),
      m_decode_loop_stopped(false), m_abort(false), m_front_pos(0),
      m_datas_bytes_available(0) {}

DecodeQueue::~DecodeQueue() { stop();clear(); }

void DecodeQueue::start() {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_abort.store(false);
  m_decode_loop_stopped.store(false);
  m_decode_thread = std::thread([this]() { decode_loop(); });
}

void DecodeQueue::clear() {
  std::unique_lock<std::mutex> lock(m_mutex);
  for (auto &data : m_frames) {
    m_decoder->freeData(&data.data);
  }
  m_frames.clear();
  m_front_pos.store(0);
  m_datas_bytes_available.store(0);
  m_cv_decode.notify_one();
  m_cv_read.notify_one();
}

void DecodeQueue::stop() {
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_abort.store(true);
    stop_loop();
    m_cv_decode.notify_one();
    m_cv_read.notify_one();
  }
  if (m_decode_thread.joinable()) {
    m_decode_thread.join();
  }
}

bool DecodeQueue::aborted() { return m_abort.load(); }

bool DecodeQueue::canRead() {
  if (aborted()) {
    return false;
  };
  return !is_empty() || !is_decode_stopped();
}


int64_t DecodeQueue::readData(uint8_t *buffer, int64_t buffer_size) {
  if (!buffer || buffer_size <= 0) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(m_mutex);
  while (is_empty()) {
    if (aborted()) {
      return 0;
    }
    m_cv_read.wait(lock, [this]() -> bool {
      return aborted() || !is_empty() || is_decode_stopped();
    });
    if (!canRead()) {
      m_cv_decode.notify_all();
      return 0;
    }
  }

  int64_t readed = 0;
  bool first = true;

  int count = 0;
  for (auto it = m_frames.begin();
       it != m_frames.end() && readed < buffer_size;) {
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

      m_decoder->freeData(&data.data);
      it = m_frames.erase(it);
      m_front_pos.store(0);
      m_datas_bytes_available.fetch_sub(size);
    } else {
      auto copyed = buffer_size - readed;
      memcpy(buffer + readed, data.data + pos, copyed);
      readed += copyed;
      m_front_pos.fetch_add(copyed);
      m_datas_bytes_available.fetch_sub(copyed);
      ++it;
    }
  }

  m_cv_decode.notify_one();
  return readed;
}

int64_t DecodeQueue::bytesAvailable() {
  return m_datas_bytes_available.load();
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
    if (!canRead()) {
      m_cv_decode.notify_all();
      return FrameData();
    }
  }
  auto data = m_frames.front();
  m_frames.pop_front();

  m_front_pos.store(0);
  m_datas_bytes_available.fetch_sub(data.size);

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
    if (aborted()) {
      m_cv_read.notify_all();
      return;
    }
  }
  for (auto &data : items) {
    m_datas_bytes_available.fetch_add(data.size);
    m_frames.push_back(std::move(data));
  }
  m_cv_read.notify_one();
}

void DecodeQueue::decode_loop() {
  while (!aborted()) {
    auto data = m_decoder->decodeNextFrameData();
    if (aborted()) {
      break;
    }

    if (data.empty()) {
      if (m_decoder->isEnd()) {
        break;
      }
      std::this_thread::yield();
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

bool DecodeQueue::is_empty() { return m_frames.empty(); }

bool DecodeQueue::is_full() { return m_frames.size() >= m_max_frames_size; }

bool DecodeQueue::is_decode_stopped() { return m_decode_loop_stopped.load(); }
