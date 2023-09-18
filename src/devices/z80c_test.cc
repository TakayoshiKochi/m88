#include "devices/z80c.h"

#include "common/io_bus.h"
#include "gtest/gtest.h"

#include <memory>

class Z80CTest : public ::testing::Test {
 public:
  Z80CTest() : cpu1_(DEV_ID('C', 'P', 'U', '1')), cpu2_(DEV_ID('C', 'P', 'U', '2')) {}
  ~Z80CTest() override = default;

  void SetUp() override {
    mem_ = std::make_unique<uint8_t[]>(0x10000);
    page_.inst = mem_.get();
    mm1_.Init(0, &page_, &page_);
    mm2_.Init(0, &page_, &page_);
    iobus1_.Init(256, nullptr);
    iobus2_.Init(256, nullptr);
    cpu1_.Init(&mm1_, &iobus1_, 0x1);
    cpu2_.Init(&mm1_, &iobus2_, 0x1);
  }
  void TearDown() override {}

 protected:
  MemoryPage page_;
  MemoryManager mm1_;
  MemoryManager mm2_;
  IOBus iobus1_;
  IOBus iobus2_;
  Z80C cpu1_;
  Z80C cpu2_;
  std::unique_ptr<uint8_t[]> mem_;
};

TEST_F(Z80CTest, Test1) {
  cpu1_.Reset();
  // cpu_.Exec(1);
  EXPECT_EQ(0, 0);
}