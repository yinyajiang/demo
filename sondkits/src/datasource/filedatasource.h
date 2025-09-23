
#pragma once
#include "audiofilter.h"
#include "datasource.h"
#include <QFile>
#include <memory>

class FileDataSource : public DataSource {
public:
  FileDataSource(int64_t frame_size,
                 const std::string &file_path);
  ~FileDataSource();

  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;
  bool realIsEnd() const override;
  void realClear() override{};

private:
  QFile m_file;
};
