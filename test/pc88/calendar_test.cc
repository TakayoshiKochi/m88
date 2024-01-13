#include "pc88/calendar.h"

#include "common/io_bus.h"
#include "gtest/gtest.h"

namespace pc8801 {

TEST(CalendarTest, BasicTest) {
  Calendar cal(DEV_ID('C', 'A', 'L', 'N'));

  EXPECT_TRUE(cal.Init());
  // TODO: nonsense
  EXPECT_EQ(cal.GetID(), DEV_ID('C', 'A', 'L', 'N'));
  // TODO: write test for shift register
}

}  // namespace pc8801