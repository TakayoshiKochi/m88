#include "pc88/beep.h"

#include "common/io_bus.h"
#include "gtest/gtest.h"

namespace pc8801 {

TEST(BeepTest, BasicTest) {
  Beep beep(DEV_ID('B', 'E', 'E', 'P'));

  EXPECT_TRUE(beep.Init());
  // TODO: nonsense
  EXPECT_EQ(beep.GetID(), DEV_ID('B', 'E', 'E', 'P'));
}
}  // namespace pc8801