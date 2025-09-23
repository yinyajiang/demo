
#include "decodequeue.h"
#include "decoder.h"
#include <mutex>

DecodeQueue::DecodeQueue(std::shared_ptr<DecoderInterface> decoder,
                         int max_frames_size)
    : m_decoder(decoder), m_max_frames_size(max_frames_size),
      m_decode_thread_stoped(false), m_abort(false), m_front_pos(0),
      m_datas_bytes_available(0), m_decoder_end(false), m_decode_semaphore(0) {
      m_decoder->setResumeDecodableCallback([this]() { resumeDecodableCallback(); });
 }

DecodeQueue::~DecodeQueue() { stop();clear(); }

void DecodeQueue::start() {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_abort.store(false);
  m_decoder_end.store(false);
  m_decode_thread_stoped.store(false);
  m_decode_thread = std::thread([this]() { decodeLoop(); });
}

void DecodeQueue::clear() {
  std::unique_lock<std::mutex> lock(m_mutex);
  for (auto &data : m_frames) {
    m_decoder->freeData(&data.data);
  }
  m_frames.clear();
  m_front_pos.store(0);
  m_datas_bytes_available.store(0);
  wakeup(false);
}

void DecodeQueue::stop() {
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_abort.store(true);
    stopLoop();
    wakeup(true);
  }
  if (m_decode_thread.joinable()) {
    m_decode_thread.join();
  }
}

bool DecodeQueue::aborted() { return m_abort.load(); }

bool DecodeQueue::readEnded() {
  if (aborted()) {
    return true;
  };
  return (isEmpty() && (m_decoder_end.load() || m_decode_thread_stoped.load()));
}


int64_t DecodeQueue::readData(uint8_t *buffer, int64_t buffer_size) {
  if (!buffer || buffer_size <= 0) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(m_mutex);
  while (isEmpty()) {
    if (aborted()) {
      return 0;
    }
    m_cv_read.wait(lock, [this]() -> bool {
      return aborted() || !isEmpty() || isDecodeNotWorded();
    });
    if (readEnded()) {
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
  while (isEmpty()) {
    if (aborted()) {
      return FrameData();
    }
    m_cv_read.wait(lock, [this]() -> bool {
      return aborted() || !isEmpty() || isDecodeNotWorded();
    });
    if (readEnded()) {
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
  while (isFull()) {
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

void DecodeQueue::decodeLoop() {
  while (!aborted()) {
    auto data = m_decoder->decodeNextFrameData();
    if (aborted()) {
      break;
    }

    if (data.empty()) {
      if (m_decoder->isEnd()) {
        m_decoder_end.store(true);
        m_cv_read.notify_all();
        m_cv_decode.notify_all();
        //等待恢复解码状态
        m_decode_semaphore.acquire();
        continue;
      }
      std::this_thread::yield();
      continue;
    } else {
      m_decoder_end.store(false);
    }
    push(std::move(data));
  }
  stopLoop();
}

void DecodeQueue::wakeup(bool include_decode) {
  m_cv_read.notify_all();
  m_cv_decode.notify_all();
  if (include_decode) {
    m_decode_semaphore.release();
  }
}

void DecodeQueue::resumeDecodableCallback() {
  wakeup(true);
}

void DecodeQueue::stopLoop() {
  m_decode_thread_stoped.store(true);
  wakeup(true);
}

bool DecodeQueue::isEmpty() { return m_frames.empty(); }

bool DecodeQueue::isFull() { return m_frames.size() >= m_max_frames_size; }

bool DecodeQueue::isDecodeNotWorded() { return m_decoder_end.load() || m_decode_thread_stoped.load(); }
