#include "devices/psg.h"

#include "gtest/gtest.h"

TEST(PsgTest, TestRegisters) {
  PSG psg;

  psg.SetReg(0, 0x10);
  EXPECT_EQ(psg.GetReg(0), 0x10);
  psg.SetReg(1, 0x11);
  EXPECT_EQ(psg.GetReg(1), 0x11);
  psg.SetReg(2, 0x12);
  EXPECT_EQ(psg.GetReg(2), 0x12);
  psg.SetReg(3, 0x13);
  EXPECT_EQ(psg.GetReg(3), 0x13);
  psg.SetReg(4, 0x14);
  EXPECT_EQ(psg.GetReg(4), 0x14);
  psg.SetReg(5, 0x15);
  EXPECT_EQ(psg.GetReg(5), 0x15);
  psg.SetReg(6, 0x16);
  EXPECT_EQ(psg.GetReg(6), 0x16);
  psg.SetReg(7, 0x17);
  EXPECT_EQ(psg.GetReg(7), 0x17);
  psg.SetReg(8, 0x18);
  EXPECT_EQ(psg.GetReg(8), 0x18);
  psg.SetReg(9, 0x19);
  EXPECT_EQ(psg.GetReg(9), 0x19);
  psg.SetReg(10, 0x1a);
  EXPECT_EQ(psg.GetReg(10), 0x1a);
  psg.SetReg(11, 0x1b);
  EXPECT_EQ(psg.GetReg(11), 0x1b);
  psg.SetReg(12, 0x1c);
  EXPECT_EQ(psg.GetReg(12), 0x1c);
  psg.SetReg(13, 0x1d);
  EXPECT_EQ(psg.GetReg(13), 0x1d);
  psg.SetReg(14, 0x1e);
  EXPECT_EQ(psg.GetReg(14), 0x1e);
  psg.SetReg(15, 0x1f);
  EXPECT_EQ(psg.GetReg(15), 0x1f);

  psg.Reset();
  EXPECT_EQ(psg.GetReg(0), 0);
  EXPECT_EQ(psg.GetReg(7), 0xff);
  EXPECT_EQ(psg.GetReg(14), 0xff);
  EXPECT_EQ(psg.GetReg(15), 0xff);
}

TEST(PsgTest, TestClockAndRate) {
  PSG psg;
  psg.Reset();
  // 998400 = 0xf3c00 = 3993600 / 4
  // 8000 = 0x1f40
  psg.SetClock(998400, 8000);
}