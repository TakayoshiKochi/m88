#include "common/scheduler.h"

#include "common/device.h"
#include "gtest/gtest.h"

class MockScheduler : public Scheduler {
 public:
  MockScheduler() = default;
  ~MockScheduler() override = default;

 private:
  int Execute(int ticks) override { return ticks; };
  void Shorten(int ticks) override {};
  int GetTicks() override { return 0; }
};

class TestDevice : public Device {
 public:
  TestDevice(ID id) : Device(id) {}
  ~TestDevice() override = default;

  void Reset() {
    passed_arg_ = 0;
    event_received_ = false;
  }

  void IOCALL OnEvent(uint32_t arg) {
    passed_arg_ = arg;
    event_received_ = true;
  }

  [[nodiscard]] uint32_t passed_arg() const { return passed_arg_; }
  [[nodiscard]] bool event_received() const { return event_received_; }

 private:
  uint32_t passed_arg_ = 0;
  bool event_received_ = false;
};

class SchedulerTest : public testing::Test {
 public:
  SchedulerTest() : dev_(0x1) {}
  ~SchedulerTest() override = default;

  void SetUp() override {
    sched_ = new MockScheduler();
    sched_->Init();
  }

  void TearDown() override { delete sched_; }

 protected:
  Scheduler* sched_ = nullptr;
  TestDevice dev_;
};

TEST_F(SchedulerTest, MockScheduler) {
  EXPECT_EQ(sched_->GetTime(), 0);
  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 1);
}

TEST_F(SchedulerTest, TestOneShotEvent) {
  EXPECT_EQ(sched_->GetTime(), 0);
  sched_->AddEvent(2, &dev_, static_cast<IDevice::TimeFunc>(&TestDevice::OnEvent), 0x123, false);
  EXPECT_FALSE(dev_.event_received());

  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 1);
  EXPECT_FALSE(dev_.event_received());

  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 2);
  EXPECT_TRUE(dev_.event_received());
  EXPECT_EQ(dev_.passed_arg(), 0x123);

  // Check if the event is not repeating
  dev_.Reset();  // clear event flag
  sched_->Proceed(2);
  EXPECT_FALSE(dev_.event_received());
}

TEST_F(SchedulerTest, TestRepeatedEvent) {
  EXPECT_EQ(sched_->GetTime(), 0);
  sched_->AddEvent(2, &dev_, static_cast<IDevice::TimeFunc>(&TestDevice::OnEvent), 0, true);
  EXPECT_FALSE(dev_.event_received());

  sched_->Proceed(2);
  EXPECT_EQ(sched_->GetTime(), 2);
  EXPECT_TRUE(dev_.event_received());

  // Check if the event is not repeating
  dev_.Reset();  // clear event flag
  sched_->Proceed(2);
  EXPECT_TRUE(dev_.event_received());
}

TEST_F(SchedulerTest, TestOneShotEventDeletion) {
  EXPECT_EQ(sched_->GetTime(), 0);
  SchedulerEvent* ev = sched_->AddEvent(2, &dev_, static_cast<IDevice::TimeFunc>(&TestDevice::OnEvent), 0x123, false);
  EXPECT_FALSE(dev_.event_received());

  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 1);
  EXPECT_FALSE(dev_.event_received());

  sched_->DelEvent(&dev_);
  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 2);
  EXPECT_FALSE(dev_.event_received());
}

TEST_F(SchedulerTest, TestOneShotEventDeletion2) {
  EXPECT_EQ(sched_->GetTime(), 0);
  SchedulerEvent* ev = sched_->AddEvent(2, &dev_, static_cast<IDevice::TimeFunc>(&TestDevice::OnEvent), 0x123, false);
  EXPECT_FALSE(dev_.event_received());

  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 1);
  EXPECT_FALSE(dev_.event_received());

  sched_->DelEvent(ev);
  sched_->Proceed(1);
  EXPECT_EQ(sched_->GetTime(), 2);
  EXPECT_FALSE(dev_.event_received());
}