
#pragma once
#include "audiofilter.h"
#include "datasource.h"
#include <QFile>
#include <memory>

class FileDataSource : public DataSource {
public:
  FileDataSource(std::shared_ptr<AudioFilter> audio_filter, int64_t frame_size,
                 const std::string &file_path);

  void open() override;
  void close() override;
  bool isEnd() const override;
  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;

private:
  QFile m_file;
};
