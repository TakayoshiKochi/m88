// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  画面制御とグラフィックス画面合成
// ---------------------------------------------------------------------------
//  $Id: screen.h,v 1.17 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include "win32/types.h"
#include "common/device.h"
#include "common/draw.h"
#include "pc88/config.h"

// ---------------------------------------------------------------------------
//  color mode
//  BITMAP BIT 配置       -- GG GR GB TE TG TR TB
//  ATTR BIT 配置     G  R  B  CG UL OL SE RE
//
//  b/w mode
//  BITMAP BIT 配置       -- -- G  RE TE TG TB TR
//  ATTR BIT 配置     G  R  B  CG UL OL SE RE
//
namespace PC8801 {

class Memory;
class CRTC;

// ---------------------------------------------------------------------------
//  88 の画面に関するクラス
//
class Screen : public Device {
 public:
  enum IDOut { reset = 0, out30, out31, out32, out33, out52, out53, out54, out55to5b };

 public:
  Screen(const ID& id);
  ~Screen();

  bool Init(IOBus* bus, Memory* memory, CRTC* crtc);
  void IOCALL Reset(uint = 0, uint = 0);
  bool UpdatePalette(Draw* draw);
  void UpdateScreen(uint8_t* image, int bpl, Draw::Region& region, bool refresh);
  void ApplyConfig(const Config* config);

  void IOCALL Out30(uint port, uint data);
  void IOCALL Out31(uint port, uint data);
  void IOCALL Out32(uint port, uint data);
  void IOCALL Out33(uint port, uint data);
  void IOCALL Out52(uint port, uint data);
  void IOCALL Out53(uint port, uint data);
  void IOCALL Out54(uint port, uint data);
  void IOCALL Out55to5b(uint port, uint data);

  const Descriptor* IFCALL GetDesc() const { return &descriptor; }

  uint IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

 private:
  struct Pal {
    uint8_t red, blue, green, _pad;
  };
  enum {
    ssrev = 1,
  };
  struct Status {
    uint rev;
    Pal pal[8], bgpal;
    uint8_t p30, p31, p32, p33, p53;
  };

  void CreateTable();

  void ClearScreen(uint8_t* image, int bpl);
  void UpdateScreen200c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen200b(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen400b(uint8_t* image, int bpl, Draw::Region& region);

  void UpdateScreen80c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen80b(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen320c(uint8_t* image, int bpl, Draw::Region& region);
  void UpdateScreen320b(uint8_t* image, int bpl, Draw::Region& region);

  IOBus* bus;
  Memory* memory;
  CRTC* crtc;

  Pal pal[8];
  Pal bgpal;
  int prevgmode;
  int prevpmode;

  static const Draw::Palette palcolor[8];

  const uint8_t* pex;

  uint8_t port30;
  uint8_t port31;
  uint8_t port32;
  uint8_t port33;
  uint8_t port53;

  bool fullline;
  bool fv15k;
  bool line400;
  bool line320;  // 320x200 mode
  uint8_t displayplane;
  bool displaytext;
  bool palettechanged;
  bool modechanged;
  bool color;
  bool displaygraphics;
  bool texttp;
  bool n80mode;
  bool textpriority;
  bool grphpriority;
  uint8_t gmask;
  Config::BASICMode newmode;

  static packed BETable0[1 << sizeof(packed)];
  static packed BETable1[1 << sizeof(packed)];
  static packed BETable2[1 << sizeof(packed)];
  static packed E80Table[1 << sizeof(packed)];
  static packed E80SRTable[64];
  static packed E80SRMask[4];
  static packed BE80Table[4];
  static const uint8_t palextable[2][8];

 private:
  static const Descriptor descriptor;
  //  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
  static const int16_t RegionTable[];
};

}  // namespace PC8801
