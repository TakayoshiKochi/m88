#include "pc88/base.h"

#include "common/draw.h"
#include "gtest/gtest.h"
#include "services/diskmgr.h"
#include "pc88/pc88.h"
#include "services/tapemgr.h"

class DummyDraw : public Draw {
 public:
  DummyDraw() = default;
  ~DummyDraw() = default;

  bool Init(uint32_t width, uint32_t height, uint32_t bpp) override { return true; };
  bool CleanUp() override { return true; };

  bool Lock(uint8_t** pimage, int* pbpl) override { return true; };
  bool Unlock() override { return true; };

  uint32_t GetStatus() override { return 0; };
  void Resize(uint32_t width, uint32_t height) override{};
  void DrawScreen(const Region& region) override{};
  void SetPalette(uint32_t index, uint32_t nents, const Palette* pal) override{};
  bool SetFlipMode(bool) override { return true; };
};

namespace pc8801 {

TEST(BaseTest, PortsTest) {
  PC88 pc88;
  DummyDraw draw;
  services::DiskManager disk_manager;
  services::TapeManager tape_manager;
  pc88.Init(&draw, &disk_manager, &tape_manager);
  Base* base = pc88.GetBase();

  EXPECT_EQ(base->GetID(), DEV_ID('B', 'A', 'S', 'E'));
  EXPECT_EQ(base->GetBasicMode(), BasicMode::kN88V1);
  // TODO: Checking after reset value isn't useful tests.
  EXPECT_EQ(base->In30(0), 0xcb);
  EXPECT_EQ(base->In31(0), 0x79);
  EXPECT_EQ(base->In40(0), 0xd5);
  EXPECT_EQ(base->In6e(0), 0xff);

  // VRTC() changes VRTC bit in port 40.
  uint32_t port40 = base->In40(0);
  EXPECT_EQ(port40 & 0x20, 0);
  base->VRTC(0, 1);
  EXPECT_EQ(base->In40(0), port40 | 0x20);
  base->VRTC(0, 0);
  EXPECT_EQ(base->In40(0), port40);
}
}  // namespace pc8801
