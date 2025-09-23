
#pragma once
#include "datasource.h"

class MemoryDataSource : public DataSource {
public:
  MemoryDataSource(int64_t frame_size, char *data, int size);

  int64_t bytesAvailable() const override;

protected:
  int64_t realReadData(uint8_t *data, int64_t size) override;
  bool realIsEnd() const override;
  void realClear() override{};

private:
  char *m_data;
  int m_size;
  int m_pos;
};