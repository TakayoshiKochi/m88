#include "pc88/crtc.h"

#include "common/io_bus.h"
#include "gtest/gtest.h"
#include "pc88/pd8257.h"

// scheduler_test.cc
class MockScheduler : public Scheduler {
 public:
  MockScheduler() = default;
  ~MockScheduler() override = default;

 private:
  int Execute(int ticks) override { return ticks; };
  void Shorten(int ticks) override{};
  int GetTicks() override { return 0; }
};

namespace PC8801 {

class CRTCTest : public ::testing::Test {
 public:
  CRTCTest() : crtc_(DEV_ID('C', 'R', 'T', 'C')), dmac_(DEV_ID('D', 'M', 'A', 'C')) {}
  ~CRTCTest() override = default;

  void SetUp() override {
      crtc_.Init(&bus_, &sched_, &dmac_);
  }

 protected:
  CRTC crtc_;
  IOBus bus_;
  MockScheduler sched_;
  PD8257 dmac_;
};


TEST_F(CRTCTest, BasicTest) {
  // TODO: nonsense
  EXPECT_EQ(crtc_.GetID(), DEV_ID('C', 'R', 'T', 'C'));
  const IDevice::Descriptor* desc = crtc_.GetDesc();
  // EXPECT_EQ((&crtc)->*(desc->indef[0], &crtc)(0x50), 0);
}

TEST_F(CRTCTest, ExpandAttributesTest) {
  uint8_t src[10000];
  uint8_t dest[80];

  memset(&src, 0, 10000);
  memset(&dest, 0, 80);
  crtc_.ExpandAttributes(dest, src, 0);
  EXPECT_EQ(0, 0);
}

TEST_F(CRTCTest, ChangeAttrTest) {
  //auto x = crtc_.ChangeAttr(3, 2);
  EXPECT_EQ(crtc_.attr_, 0);
}


}  // namespace PC8801