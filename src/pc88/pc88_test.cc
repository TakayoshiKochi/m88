#include "pc88/pc88.h"

#include "common/status.h"
#include "gtest/gtest.h"
#include "win32/romeo/piccolo.h"

StatusDisplay* g_status_display = new StatusDisplay(new DummyStatusDisplay);

// For tests
Piccolo* Piccolo::GetInstance() {
  return nullptr;
}

// For tests
void Piccolo::DeleteInstance() {}

// ???
// GetCurrentTime() is replaced by GetTickCount() in winbase.h
uint32_t Piccolo::GetTickCount() {
  return 0;
}

namespace PC8801 {

TEST(PC88Test, Test1) {
  ::PC88 pc88;
  EXPECT_EQ(1, 1);
}

}  // namespace PC8801
