#include "devices/fmgen.h"

#include "gtest/gtest.h"

namespace FM {

TEST(FmgenTest, TestChip) {
  Chip c;
  c.SetAML(3);
  EXPECT_EQ(c.GetAML(), 3);
  c.SetPML(4);
  EXPECT_EQ(c.GetPML(), 4);
  c.SetPMV(5);
  EXPECT_EQ(c.GetPMV(), 5);
  c.SetRatio(3000);
  EXPECT_EQ(c.GetRatio(), 3000);
}

TEST(FmgenTest, TestOperator) {
  Operator o;

  o.SetDT(1);
  o.SetDT2(1);
  o.SetMULTI(3);
  o.SetTL(4, false);
  o.SetKS(1);
  o.SetAR(10);
  o.SetDR(10);
  o.SetSR(10);
  o.SetRR(10);
  o.SetSL(10);
  o.SetSSGEC(1);
  o.SetFNum(1000);
  o.SetDPBN(2, 3);
  // o.SetMode(true);
  o.SetAMON(false);
  o.SetMS(1);
  o.Mute(false);

  o.KeyOn();
  EXPECT_EQ(1, 1);
}

TEST(FmgenTest, TestChannel4) {
  Channel4 c;
  EXPECT_EQ(0, 0);
}

}  // namespace FM