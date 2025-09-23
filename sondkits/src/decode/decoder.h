#pragma once
#include <list>

struct FrameData {
  uint8_t *data;
  int size;
};
using FrameDataList = std::list<FrameData>;

using ResumeDecodableCallback = std::function<void()>;

class DecoderInterface {
public:
  virtual ~DecoderInterface() = default;
  virtual FrameDataList decodeNextFrameData() = 0;
  virtual bool isEnd() const = 0;
  virtual void freeData(uint8_t **data) = 0;
  virtual void
  setResumeDecodableCallback(ResumeDecodableCallback resume_decodable_cb) {
    m_resume_decodable_cb = resume_decodable_cb;
  }

protected:
  void resumeDecodable() {
    if (m_resume_decodable_cb && !isEnd()) {
      m_resume_decodable_cb();
    }
  }

private:
ResumeDecodableCallback m_resume_decodable_cb;
};
