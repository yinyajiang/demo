#pragma once
#include "datasource.h"
#include <memory>
#include <vector>
extern "C" {
#include <libavutil/samplefmt.h>
}

class ComposeDataSource : public DataSource {
public:
  explicit ComposeDataSource(int64_t frame_size, AVSampleFormat sample_format);
  ~ComposeDataSource() override = default;

  void addDataSource(std::shared_ptr<DataSource> data_source);

  // 获取可用字节数
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t max_size) override;
  bool realIsEnd() const override;
  void realClear() override;

private:
  std::vector<std::shared_ptr<DataSource>> m_data_sources;
  AVSampleFormat m_sample_format;
};
