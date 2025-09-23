
#pragma once
#include "audiofilter.h"
#include "datasource.h"
#include "decodequeue.h"
#include <memory>

class DecodeDataSource : public DataSource {
public:
  DecodeDataSource(std::shared_ptr<AudioFilter> audio_filter,
                   int64_t frame_size,
                   std::shared_ptr<DecodeQueue> decode_queue);

  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t maxlen) override;
  bool realIsEnd() const override;

private:
  std::shared_ptr<DecodeQueue> m_decode_queue;
};
