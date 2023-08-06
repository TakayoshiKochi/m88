#include "devices/z80c.h"

#include "common/io_bus.h"
#include "gtest/gtest.h"

#include <memory>

class Z80CTest : public ::testing::Test {
 public:
  Z80CTest() : cpu_(0x1) {}
  ~Z80CTest() override = default;

  void SetUp() override {
    mem_ = std::make_unique<uint8_t[]>(0x10000);
    page_.inst = mem_.get();
    mm_.Init(0, &page_, &page_);
    cpu_.Init(&mm_, &iobus_, 0x1);
  }
  void TearDown() override {}

 protected:
  MemoryPage page_;
  MemoryManager mm_;
  IOBus iobus_;
  Z80C cpu_;
  std::unique_ptr<uint8_t[]> mem_;
};

TEST_F(Z80CTest, Test1) {
  cpu_.Reset();
  // cpu_.Exec(1);
  EXPECT_EQ(0, 0);
}