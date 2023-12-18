#include "common/crc32.h"

#include "gtest/gtest.h"

namespace {

constexpr uint32_t* make_table() {
  uint32_t crctable[256];

  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t r = i << 24;
    for (int j = 0; j < 8; ++j)
      r = (r << 1) ^ (r & 0x80000000 ? 0x04c11db7 : 0);
    crctable[i] = r;
  }

  return crctable;
}

// reference implementation
uint32_t calc(uint8_t* data, int len) {
  static uint32_t* crctable = make_table();

  uint32_t crc = 0xffffffff;
  for (int i = 0; i < len; i++)
    crc = (crc << 8) ^ crctable[((crc >> 24) ^ data[i]) & 0xff];

  crc = ~crc;
  return crc;
}

} // namespace

TEST(CRC32Test, TestCRC) {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};  // => 0x86c8c832
  EXPECT_EQ(CRC32::Calc(data, sizeof(data)), calc(data, sizeof(data)));
}
