#include "pc88/pc88.h"

#include "common/status_bar.h"
#include "gtest/gtest.h"
#include "win32/romeo/piccolo.h"

class DummyStatusDisplay : public StatusBar {
 public:
  void UpdateDisplay() override {}
  void Update() override {}
};

StatusBar* g_status_bar = new DummyStatusDisplay();

// static
std::once_flag Piccolo::once_;
Piccolo* Piccolo::instance = nullptr;

// For tests
void Piccolo::InitHardware() {}

// For tests
void Piccolo::DeleteInstance() {}

// ???
// GetCurrentTime() is replaced by GetTickCount() in winbase.h
uint32_t Piccolo::GetCurrentTimeUS() {
  return 0;
}

namespace pc8801 {

TEST(PC88Test, Test1) {
  ::PC88 pc88;
  EXPECT_EQ(1, 1);
}

}  // namespace pc8801
