#pragma once
#include <list>

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
