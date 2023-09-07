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
  void Shorten(int ticks) override {};
  int GetTicks() override { return 0; }
};

namespace PC8801 {

TEST(crtcTest, BasicTest) {
  CRTC crtc(DEV_ID('C', 'R', 'T', 'C'));
  IOBus bus;
  MockScheduler sched;
  PD8257 dmac(DEV_ID('D', 'M', 'A', 'C'));

  EXPECT_TRUE(crtc.Init(&bus, &sched, &dmac));
  // TODO: nonsense
  EXPECT_EQ(crtc.GetID(), DEV_ID('C', 'R', 'T', 'C'));
  const IDevice::Descriptor* desc = crtc.GetDesc();
  // EXPECT_EQ((&crtc)->*(desc->indef[0], &crtc)(0x50), 0);
}
}  // namespace PC8801