#include "common/crc32.h"

// static
std::once_flag CRC32::once_;
uint32_t CRC32::table_[256];

uint32_t CRC32::Calc(const uint8_t* data, size_t size) {
  std::call_once(once_, MakeTable);

  uint32_t crc = ~0;
  for (int i = 0; i < size; ++i) {
    crc = (crc << 8) ^ table_[((crc >> 24) ^ data[i]) & 0xff];
  }
  return ~crc;
}

// static
void CRC32::MakeTable() {
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = i << 24;
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x80000000) {
        crc = (crc << 1) ^ crc32_poly;
      } else {
        crc <<= 1;
      }
    }
    table_[i] = crc;
  }
}
