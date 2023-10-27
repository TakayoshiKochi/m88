// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  画面制御とグラフィックス画面合成
// ---------------------------------------------------------------------------
//  $Id: screen.h,v 1.17 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include <stdint.h>

#include "common/device.h"
#include "common/draw.h"
#include "pc88/config.h"

class IOBus;

// ---------------------------------------------------------------------------
//  color mode
//  BITMAP BIT 配置       -- GG GR GB TE TG TR TB
//  ATTR BIT 配置     G  R  B  CG UL OL SE RE
//
//  b/w mode
//  BITMAP BIT 配置       -- -- G  RE TE TG TB TR
//  ATTR BIT 配置     G  R  B  CG UL OL SE RE
//
namespace pc8801 {

class CRTC;
class Memory;

// ---------------------------------------------------------------------------
//  88 の画面に関するクラス
//
class Screen : public Device {
 public:
  enum IDOut { reset = 0, out30, out31, out32, out33, out52, out53, out54, out55to5b };

 public:
  explicit Screen(const ID& id);
  ~Screen() override;

  bool Init(IOBus* bus, Memory* memory, CRTC* crtc);
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  bool UpdatePalette(Draw* draw);
  void UpdateScreen(uint8_t* image, int bpl, Draw::Region& region, bool refresh);
  void ApplyConfig(const Config* config);

  // I/O
  void IOCALL Out30(uint32_t port, uint32_t data);
  void IOCALL Out31(uint32_t port, uint32_t data);
  void IOCALL Out32(uint32_t port, uint32_t data);
  void IOCALL Out33(uint32_t port, uint32_t data);
  void IOCALL Out52(uint32_t port, uint32_t data);
  void IOCALL Out53(uint32_t port, uint32_t data);
  void IOCALL Out54(uint32_t port, uint32_t data);
  void IOCALL Out55to5b(uint32_t port, uint32_t data);

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

 private:
  enum {
    ssrev = 1,
  };
  struct Status {
    uint32_t rev;
    Draw::Palette pal[8], bgpal;
    uint8_t p30;
    uint8_t p31;
    uint8_t p32;
    uint8_t p33;
    uint8_t p53;
  };

  static void CreateTable();

  void ClearScreen(uint8_t* image, int bpl) const;
  void UpdateScreen200c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen200b(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen400b(uint8_t* image, int bpl, Draw::Region& region);

  void UpdateScreen80c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen80b(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen320c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen320b(uint8_t* image, int bpl, Draw::Region& region);

  IOBus* bus_ = nullptr;
  Memory* memory_ = nullptr;
  CRTC* crtc_ = nullptr;

  Draw::Palette pal_[8]{};
  Draw::Palette bg_pal_{};
  int prev_gmode_ = 0;
  int prev_pmode_ = 0;

  const uint8_t* pex_ = nullptr;

  uint8_t port30_ = 0;
  uint8_t port31_ = 0;
  uint8_t port32_ = 0;
  uint8_t port33_ = 0;
  uint8_t port53_ = 0;

  bool full_line_ = false;
  bool fv15k_ = false;
  bool line400_ = false;
  bool line320_ = false;  // 320x200 mode
  uint8_t display_plane_ = 0;
  bool display_text_ = false;
  bool palette_changed_ = true;
  bool mode_changed_ = true;
  bool color_ = false;
  bool display_graphics_ = false;
  bool text_tp_ = false;
  bool n80mode_ = false;
  bool text_priority_ = false;
  bool grph_priority_ = false;
  uint8_t gmask_ = 0;
  BasicMode newmode_ = BasicMode::kN88V1;

 private:
  static const Descriptor descriptor;
  //  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
