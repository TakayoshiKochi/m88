

#include "common/scheduler.h"

#include "gtest/gtest.h"

class TestScheduler : public Scheduler {
 public:
  TestScheduler() = default;
  ~TestScheduler() = default;

 private:
  virtual int Execute(int ticks) { return 0; };
  virtual void Shorten(int ticks){};
  virtual int GetTicks() { return 0; }
};

class SchedulerTest : public testing::Test {
 public:
  SchedulerTest() = default;
  ~SchedulerTest() = default;

  void SetUp() override {
    sched_ = new TestScheduler();
    sched_->Init();
  }

  void TearDown() override { delete sched_; }

 private:
  Scheduler* sched_;
};

TEST_F(SchedulerTest, TestId) {
  EXPECT_EQ(0, 0);
}
