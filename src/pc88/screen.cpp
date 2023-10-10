// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  画面制御とグラフィックス画面合成
// ---------------------------------------------------------------------------
//  $Id: screen.cpp,v 1.26 2003/09/28 14:35:35 cisc Exp $

#include "pc88/screen.h"

#include "common/io_bus.h"
#include "pc88/config.h"
#include "pc88/crtc.h"
#include "pc88/memory.h"

// #define LOGNAME "screen"
#include "common/diag.h"

using namespace PC8801;

namespace {
constexpr uint8_t GVRAMC_BIT = 0xf0;
constexpr uint8_t GVRAMC_CLR = 0xc0;
constexpr uint8_t GVRAM0_SET = 0x10;
constexpr uint8_t GVRAM0_RES = 0x00;
constexpr uint8_t GVRAM1_SET = 0x20;
constexpr uint8_t GVRAM1_RES = 0x00;
constexpr uint8_t GVRAM2_SET = 0x80;
constexpr uint8_t GVRAM2_RES = 0x40;

constexpr uint8_t GVRAMM_BIT = 0x20;   // 1110
constexpr uint8_t GVRAMM_BITF = 0xe0;  // 1110
constexpr uint8_t GVRAMM_SET = 0x20;
constexpr uint8_t GVRAMM_ODD = 0x40;
constexpr uint8_t GVRAMM_EVEN = 0x80;

packed BETable0[1 << sizeof(packed)] = {-1};
packed BETable1[1 << sizeof(packed)];
packed BETable2[1 << sizeof(packed)];
packed E80Table[1 << sizeof(packed)];
packed E80SRTable[64];
packed E80SRMask[4];
packed BE80Table[4];

constexpr int16_t RegionTable[64] = {
    640, -1,  0, 128, 128, 256, 0, 256, 256, 384, 0, 384, 128, 384, 0, 384,
    384, 512, 0, 512, 128, 512, 0, 512, 256, 512, 0, 512, 128, 512, 0, 512,

    512, 640, 0, 640, 128, 640, 0, 640, 256, 640, 0, 640, 128, 640, 0, 640,
    384, 640, 0, 640, 128, 640, 0, 640, 256, 640, 0, 640, 128, 640, 0, 640,
};

// ---------------------------------------------------------------------------
//  原色パレット
//  RGB
constexpr Draw::Palette palcolor[8] = {
    {0, 0, 0},   {0, 0, 255},   {255, 0, 0},   {255, 0, 255},
    {0, 255, 0}, {0, 255, 255}, {255, 255, 0}, {255, 255, 255},
};

constexpr uint8_t palextable[2][8] = {
    {0, 36, 73, 109, 146, 182, 219, 255},
    {0, 255, 255, 255, 255, 255, 255, 255},
};
}  // namespace

// ---------------------------------------------------------------------------
// 構築/消滅
//
Screen::Screen(const ID& id) : Device(id) {
  CreateTable();
}

Screen::~Screen() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool Screen::Init(IOBus* bus, Memory* mem, CRTC* crtc) {
  bus_ = bus;
  memory_ = mem;
  crtc_ = crtc;

  palette_changed_ = true;
  mode_changed_ = true;
  fv15k_ = false;
  text_tp_ = false;
  pex_ = palextable[0];

  bg_pal_.red = 0;
  bg_pal_.green = 0;
  bg_pal_.blue = 0;
  gmask_ = 0;
  for (int c = 0; c < 8; ++c) {
    pal_[c].green = c & 4 ? 255 : 0;
    pal_[c].red = c & 2 ? 255 : 0;
    pal_[c].blue = c & 1 ? 255 : 0;
  }
  return true;
}

void IOCALL Screen::Reset(uint32_t, uint32_t) {
  n80mode_ = (static_cast<uint32_t>(newmode_) & 2) != 0;
  palette_changed_ = true;
  display_graphics_ = false;
  text_priority_ = false;
  grph_priority_ = false;
  port31_ = ~port31_;
  Out31(0x31, ~port31_);
  mode_changed_ = true;
}

static inline Draw::Palette Avg(Draw::Palette a, Draw::Palette b) {
  Draw::Palette c;
  c.red = (a.red + b.red) / 2;
  c.green = (a.green + b.green) / 2;
  c.blue = (a.blue + b.blue) / 2;
  return c;
}

// ---------------------------------------------------------------------------
//  パレットを更新
//
bool Screen::UpdatePalette(Draw* draw) {
  // 53 53 53 V2 32 31 80  CM 53 30 53 53 53 30 dg
  int pmode = display_graphics_ ? 1 : 0;
  pmode |= (port53_ & 1) << 6;
  pmode |= port30_ & 0x22;
  pmode |= n80mode_ ? 0x100 : 0;
  pmode |= line320_ ? 0x400 : 0;
  pmode |= (port33_ & 0x80) ? 0x800 : 0;
  if (!color_) {
    pmode |= 0x80 | ((port53_ & 14) << 1);
    if (n80mode_ && (port33_ & 0x80) && line320_)  // 80SR 320x200x6
      pmode |= (port53_ & 0x70) << 8;
  } else {
    if (n80mode_ && (port33_ & 0x80))
      pmode |= (port53_ & (line320_ ? 6 : 2)) << 1;
  }
  //  g_status_display->Show(10, 0, "SCRN: %.3x", pmode);

  if (pmode != prev_pmode_ || mode_changed_) {
    Log("p:%.2x ", pmode);
    palette_changed_ = true;
    prev_pmode_ = pmode;
  }

  if (palette_changed_) {
    palette_changed_ = false;
    // palette parameter is
    //  palette
    //  -textcolor(port30 & 2)
    //  -displaygraphics
    //  port32 & 0x20
    //  -port53 & 1
    //  ^port53 & 6 (if not color)

    Draw::Palette xpal[10]{};
    if (!text_tp_) {
      for (int i = 0; i < 8; ++i) {
        xpal[i].red = pex_[pal_[i].red];
        xpal[i].green = pex_[pal_[i].green];
        xpal[i].blue = pex_[pal_[i].blue];
      }
    } else {
      for (int i = 0; i < 8; ++i) {
        xpal[i].red = (pex_[pal_[i].red] * 3 + ((i << 7) & 0x100)) / 4;
        xpal[i].green = (pex_[pal_[i].green] * 3 + ((i << 6) & 0x100)) / 4;
        xpal[i].blue = (pex_[pal_[i].blue] * 3 + ((i << 8) & 0x100)) / 4;
      }
    }
    if (gmask_) {
      for (int i = 0; i < 8; ++i) {
        if (i & ~gmask_) {
          xpal[i].green = (xpal[i].green / 8) + 0xe0;
          xpal[i].red = (xpal[i].red / 8) + 0xe0;
          xpal[i].blue = (xpal[i].blue / 8) + 0xe0;
        } else {
          xpal[i].green = (xpal[i].green / 6) + 0;
          xpal[i].red = (xpal[i].red / 6) + 0;
          xpal[i].blue = (xpal[i].blue / 6) + 0;
        }
      }
    }

    xpal[8].red = xpal[8].green = xpal[8].blue = 0;
    xpal[9].red = pex_[bg_pal_.red];
    xpal[9].green = pex_[bg_pal_.green];
    xpal[9].blue = pex_[bg_pal_.blue];

    Draw::Palette palette[0x90];
    Draw::Palette* p = palette;

    int textcolor = port30_ & 2 ? 7 : 0;

    if (color_) {
      Log("\ncolor  port53 = %.2x  port32 = %.2x\n", port53_, port32_);
      //  color mode      GG GG GR GB TE TG TR TB
      if (port53_ & 1)  // hide text plane ?
      {
        for (int gc = 0; gc < 9; ++gc) {
          Draw::Palette c = display_graphics_ || text_tp_ ? xpal[gc] : xpal[8];

          for (int i = 0; i < 16; ++i)
            *p++ = c;
        }
      } else {
        for (int gc = 0; gc < 9; ++gc) {
          Draw::Palette c = display_graphics_ || text_tp_ ? xpal[gc] : xpal[8];

          for (int i = 0; i < 8; ++i)
            *p++ = c;
          if (text_priority_ && gc > 0) {
            for (int tc = 0; tc < 8; ++tc)
              *p++ = c;
          } else if (text_tp_) {
            for (int tc = 0; tc < 8; ++tc)
              *p++ = Avg(c, palcolor[tc]);
          } else {
            *p++ = palcolor[0];
            for (int tc = 1; tc < 8; ++tc)
              *p++ = palcolor[tc | textcolor];
          }
        }

        if (fv15k_) {
          for (int i = 0x80; i < 0x90; ++i)
            palette[i] = palcolor[0];
        }
      }
    } else {
      //  b/w mode    0  1  G  RE TE   TG TB TR
      const Draw::Palette* tpal = port32_ & 0x20 ? xpal : palcolor;

      Log("\nb/w  port53 = %.2x  port32 = %.2x  port30 = %.2x\n", port53_, port32_, port30_);
      if (port53_ & 1) {  // hidetext
        int m = text_tp_ || display_graphics_ ? ~0 : ~1;
        for (int gc = 0; gc < 4; ++gc) {
          int x = gc & m;
          if (((~x >> 1) & x & 1)) {
            for (int i = 0; i < 32; ++i)
              *p++ = tpal[(i & 7) | textcolor];
          } else {
            for (int i = 0; i < 32; ++i)
              *p++ = xpal[9];
          }
        }
      } else {
        int m = text_tp_ || display_graphics_ ? ~0 : ~4;
        for (int gc = 0; gc < 16; ++gc) {
          int x = gc & m;
          if (((((~x >> 3) & (x >> 2)) | x) ^ (x >> 1)) & 1) {
            if ((x & 8) && fv15k_)
              for (int i = 0; i < 8; ++i)
                *p++ = xpal[8];
            else
              for (int i = 0; i < 8; ++i)
                *p++ = tpal[i | textcolor];
          } else {
            for (int i = 0; i < 8; ++i)
              *p++ = xpal[9];
          }
        }
      }
    }

    //      for (int gc=0; gc<0x90; ++gc)
    //          Log("P[%.2x] = %.2x %.2x %.2x\n", gc, palette[gc].green, palette[gc].red,
    //          palette[gc].blue);
    draw->SetPalette(0x40, 0x90, palette);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  画面イメージの更新
//  arg:    region      更新領域
//
void Screen::UpdateScreen(uint8_t* image, int bpl, Draw::Region& region, bool refresh) {
  // 53 53 53 GR TX  80 V2 32 CL  53 53 53 L4 (b4〜b6の配置は変えないこと)
  int gmode = line400_ ? 1 : 0;
  gmode |= color_ ? 0x10 : (port53_ & 0x0e);
  gmode |= line320_ ? 0x20 : 0;
  gmode |= (port33_ & 0x80) ? 0x40 : 0;
  gmode |= n80mode_ ? 0x80 : 0;
  gmode |= text_priority_ ? 0x100 : 0;
  gmode |= grph_priority_ ? 0x200 : 0;
  if (n80mode_ && (port33_ & 0x80)) {
    if (color_)
      gmode |= port53_ & (line320_ ? 6 : 2);
    else if (line320_)
      gmode |= (port53_ & 0x70) << 6;
  }

  if (gmode != prev_gmode_) {
    Log("g:%.2x ", gmode);
    prev_gmode_ = gmode;
    mode_changed_ = true;
  }

  if (mode_changed_ || refresh) {
    Log("<modechange> ");
    mode_changed_ = false;
    palette_changed_ = true;
    ClearScreen(image, bpl);
    memset(memory_->GetDirtyFlag(), 1, 0x400);
  }
  if (!n80mode_) {
    if (color_)
      UpdateScreen200c(image, bpl, region);
    else {
      if (line400_)
        UpdateScreen400b(image, bpl, region);
      else
        UpdateScreen200b(image, bpl, region);
    }
  } else {
    switch ((gmode >> 4) & 7) {
      case 0:
        UpdateScreen80b(image, bpl, region);
        break;  // V1 640x200 B&W
      case 1:
        UpdateScreen200c(image, bpl, region);
        break;  // V1 640x200 COLOR
      case 2:
        break;  // V1 320x200 は常にCOLORなのでB&Wは存在しない
      case 3:
        UpdateScreen80c(image, bpl, region);
        break;  // V1 320x200 COLOR
      case 4:
        UpdateScreen200b(image, bpl, region);
        break;  // V2 640x200 B&W
      case 5:
        UpdateScreen200c(image, bpl, region);
        break;  // V2 640x200 COLOR
      case 6:
        UpdateScreen320b(image, bpl, region);
        break;  // V2 320x200 B&W
      case 7:
        UpdateScreen320c(image, bpl, region);
        break;  // V2 320x200 COLOR
    }
  }
}

// ---------------------------------------------------------------------------
//  画面更新
//
#define WRITEC0(d, a)                                                                \
  d = (d & ~PACK(GVRAMC_BIT)) | BETable0[(a >> 4) & 15] | BETable1[(a >> 12) & 15] | \
      BETable2[(a >> 20) & 15]

#define WRITEC1(d, a)                                                        \
  d = (d & ~PACK(GVRAMC_BIT)) | BETable0[a & 15] | BETable1[(a >> 8) & 15] | \
      BETable2[(a >> 16) & 15]

#define WRITEC0F(o, a)                                                                  \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] =                                      \
      (d[o] & ~PACK(GVRAMC_BIT)) | BETable0[(a >> 4) & 15] | BETable1[(a >> 12) & 15] | \
      BETable2[(a >> 20) & 15]

#define WRITEC1F(o, a)                                                                             \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = (d[o] & ~PACK(GVRAMC_BIT)) | BETable0[a & 15] | \
                                                   BETable1[(a >> 8) & 15] |                       \
                                                   BETable2[(a >> 16) & 15]

// 640x200, 3 plane color
void Screen::UpdateScreen200c(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty = memory_->GetDirtyFlag();
  int y;
  for (y = 0; y < 1000; y += sizeof(packed)) {
    if (*(packed*)(&dirty[y]))
      break;
  }
  if (y < 1000) {
    y /= 5;

    int begin = y, end = y;

    image += 2 * bpl * y;
    dirty += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;
    int dm = 0;

    if (!full_line_) {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITEC0(d[0], s[0].pack);
              WRITEC1(d[1], s[0].pack);
              WRITEC0(d[2], s[1].pack);
              WRITEC1(d[3], s[1].pack);
              WRITEC0(d[4], s[2].pack);
              WRITEC1(d[5], s[2].pack);
              WRITEC0(d[6], s[3].pack);
              WRITEC1(d[7], s[3].pack);
              d += 8, s += 4;
            }
          }
        }
      }
    } else {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITEC0F(0, s[0].pack);
              WRITEC1F(1, s[0].pack);
              WRITEC0F(2, s[1].pack);
              WRITEC1F(3, s[1].pack);
              WRITEC0F(4, s[2].pack);
              WRITEC1F(5, s[2].pack);
              WRITEC0F(6, s[3].pack);
              WRITEC1F(7, s[3].pack);
              d += 8, s += 4;
            }
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  画面更新 (200 lines  b/w)
//
#define WRITEB0(d, a) \
  d = (d & ~PACK(GVRAMM_BIT)) | BETable1[((a >> 4) | (a >> 12) | (a >> 20)) & 15]

#define WRITEB1(d, a) d = (d & ~PACK(GVRAMM_BIT)) | BETable1[(a | (a >> 8) | (a >> 16)) & 15]

#define WRITEB0F(o, a)                             \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = \
      (d[o] & ~PACK(GVRAMM_BIT)) | BETable1[((a >> 4) | (a >> 12) | (a >> 20)) & 15]

#define WRITEB1F(o, a)                             \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = \
      (d[o] & ~PACK(GVRAMM_BIT)) | BETable1[(a | (a >> 8) | (a >> 16)) & 15]

// 640x200, b/w
void Screen::UpdateScreen200b(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty = memory_->GetDirtyFlag();
  int y;
  for (y = 0; y < 1000; y += sizeof(packed)) {
    if (*(packed*)(&dirty[y]))
      break;
  }
  if (y < 1000) {
    y /= 5;

    int begin = y, end = y;

    image += 2 * bpl * y;
    dirty += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;

    Memory::Quadbyte mask;
    mask.byte[0] = port53_ & 2 ? 0x00 : 0xff;
    mask.byte[1] = port53_ & 4 ? 0x00 : 0xff;
    mask.byte[2] = port53_ & 8 ? 0x00 : 0xff;
    mask.byte[3] = 0;

    int dm = 0;
    if (!full_line_) {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              uint32_t x;
              x = s[0].pack & mask.pack;
              WRITEB0(d[0], x);
              WRITEB1(d[1], x);
              x = s[1].pack & mask.pack;
              WRITEB0(d[2], x);
              WRITEB1(d[3], x);
              x = s[2].pack & mask.pack;
              WRITEB0(d[4], x);
              WRITEB1(d[5], x);
              x = s[3].pack & mask.pack;
              WRITEB0(d[6], x);
              WRITEB1(d[7], x);
              d += 8, s += 4;
            }
          }
        }
      }
    } else {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              uint32_t x;
              x = s[0].pack & mask.pack;
              WRITEB0F(0, x);
              WRITEB1F(1, x);
              x = s[1].pack & mask.pack;
              WRITEB0F(2, x);
              WRITEB1F(3, x);
              x = s[2].pack & mask.pack;
              WRITEB0F(4, x);
              WRITEB1F(5, x);
              x = s[3].pack & mask.pack;
              WRITEB0F(6, x);
              WRITEB1F(7, x);
              d += 8, s += 4;
            }
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  画面更新 (400 lines  b/w)
//
#define WRITE400B(d, a)                                            \
  (d)[0] = ((d)[0] & ~PACK(GVRAMM_BIT)) | BETable1[(a >> 4) & 15], \
  (d)[1] = ((d)[1] & ~PACK(GVRAMM_BIT)) | BETable1[(a >> 0) & 15]

void Screen::UpdateScreen400b(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty = memory_->GetDirtyFlag();
  int y;
  for (y = 0; y < 1000; y += sizeof(packed)) {
    if (*(packed*)(&dirty[y]))
      break;
  }
  if (y < 1000) {
    y /= 5;

    int begin = y, end = y;

    image += bpl * y;
    dirty += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;

    Memory::Quadbyte mask;
    mask.byte[0] = port53_ & 2 ? 0x00 : 0xff;
    mask.byte[1] = port53_ & 4 ? 0x00 : 0xff;
    mask.byte[2] = port53_ & 8 ? 0x00 : 0xff;
    mask.byte[3] = 0;

    int dm = 0;
    for (; y < 200; ++y, image += bpl) {
      uint8_t* dest0 = image;
      uint8_t* dest1 = image + 200 * bpl;

      for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest0 += 128, dest1 += 128) {
        if (*dirty) {
          *dirty = 0;
          end = y;
          dm |= 1 << x;

          Memory::Quadbyte* s = src;
          packed* d0 = (packed*)dest0;
          packed* d1 = (packed*)dest1;
          for (int j = 0; j < 4; ++j) {
            WRITE400B(d0, s[0].byte[0]);
            WRITE400B(d1, s[0].byte[1]);
            WRITE400B(d0 + 2, s[1].byte[0]);
            WRITE400B(d1 + 2, s[1].byte[1]);
            WRITE400B(d0 + 4, s[2].byte[0]);
            WRITE400B(d1 + 4, s[2].byte[1]);
            WRITE400B(d0 + 6, s[3].byte[0]);
            WRITE400B(d1 + 6, s[3].byte[1]);
            d0 += 8, d1 += 8, s += 4;
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], begin, RegionTable[dm * 2 + 1], 200 + end);
  }
}

// ---------------------------------------------------------------------------
//  画面更新
//
#define WRITE80C0(d, a) d = (d & ~PACK(GVRAMC_BIT)) | E80Table[(a >> 4) & 15]

#define WRITE80C1(d, a) d = (d & ~PACK(GVRAMC_BIT)) | E80Table[a & 15]

#define WRITE80C0F(o, a)                           \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = \
      (d[o] & ~PACK(GVRAMC_BIT)) | E80Table[(a >> 4) & 15]
#define WRITE80C1F(o, a) \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = (d[o] & ~PACK(GVRAMC_BIT)) | E80Table[a & 15]

// 320x200, color?
void Screen::UpdateScreen80c(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty = memory_->GetDirtyFlag();
  int y;
  for (y = 0; y < 1000; y += sizeof(packed)) {
    if (*(packed*)(&dirty[y]))
      break;
  }
  if (y < 1000) {
    y /= 5;

    int begin = y, end = y;

    image += 2 * bpl * y;
    dirty += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;
    int dm = 0;

    if (!full_line_) {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITE80C0(d[0], s[0].byte[0]);
              WRITE80C1(d[1], s[0].byte[0]);
              WRITE80C0(d[2], s[1].byte[0]);
              WRITE80C1(d[3], s[1].byte[0]);
              WRITE80C0(d[4], s[2].byte[0]);
              WRITE80C1(d[5], s[2].byte[0]);
              WRITE80C0(d[6], s[3].byte[0]);
              WRITE80C1(d[7], s[3].byte[0]);
              d += 8, s += 4;
            }
          }
        }
      }
    } else {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITE80C0F(0, s[0].byte[0]);
              WRITE80C1F(1, s[0].byte[0]);
              WRITE80C0F(2, s[1].byte[0]);
              WRITE80C1F(3, s[1].byte[0]);
              WRITE80C0F(4, s[2].byte[0]);
              WRITE80C1F(5, s[2].byte[0]);
              WRITE80C0F(6, s[3].byte[0]);
              WRITE80C1F(7, s[3].byte[0]);
              d += 8, s += 4;
            }
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  画面更新 (200 lines  b/w)
//
#define WRITE80B0(d, a) d = (d & ~PACK(GVRAMM_BIT)) | BETable1[(a >> 4) & 15]

#define WRITE80B1(d, a) d = (d & ~PACK(GVRAMM_BIT)) | BETable1[(a)&15]

#define WRITE80B0F(o, a)                           \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = \
      (d[o] & ~PACK(GVRAMM_BIT)) | BETable1[(a >> 4) & 15]

#define WRITE80B1F(o, a) \
  *((packed*)(((uint8_t*)(d + o)) + bpl)) = d[o] = (d[o] & ~PACK(GVRAMM_BIT)) | BETable1[(a)&15]

void Screen::UpdateScreen80b(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty = memory_->GetDirtyFlag();
  int y;
  for (y = 0; y < 1000; y += sizeof(packed)) {
    if (*(packed*)(&dirty[y]))
      break;
  }
  if (y < 1000) {
    y /= 5;

    int begin = y, end = y;

    image += 2 * bpl * y;
    dirty += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;

    Memory::Quadbyte mask;
    if (!gmask_) {
      mask.byte[0] = port53_ & 2 ? 0x00 : 0xff;
      mask.byte[1] = port53_ & 4 ? 0x00 : 0xff;
      mask.byte[2] = port53_ & 8 ? 0x00 : 0xff;
    } else {
      mask.byte[0] = gmask_ & 1 ? 0x00 : 0xff;
      mask.byte[1] = gmask_ & 2 ? 0x00 : 0xff;
      mask.byte[2] = gmask_ & 4 ? 0x00 : 0xff;
    }
    mask.byte[3] = 0;

    int dm = 0;

    if (!full_line_) {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITE80B0(d[0], s[0].byte[0]);
              WRITE80B1(d[1], s[0].byte[0]);
              WRITE80B0(d[2], s[1].byte[0]);
              WRITE80B1(d[3], s[1].byte[0]);
              WRITE80B0(d[4], s[2].byte[0]);
              WRITE80B1(d[5], s[2].byte[0]);
              WRITE80B0(d[6], s[3].byte[0]);
              WRITE80B1(d[7], s[3].byte[0]);
              d += 8, s += 4;
            }
          }
        }
      }
    } else {
      for (; y < 200; ++y, image += 2 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty, src += 16, dest += 32) {
          if (*dirty) {
            *dirty = 0;
            end = y;
            dm |= 1 << x;

            Memory::Quadbyte* s = src;
            packed* d = (packed*)dest;
            for (int j = 0; j < 4; ++j) {
              WRITE80B0F(0, s[0].byte[0]);
              WRITE80B1F(1, s[0].byte[0]);
              WRITE80B0F(2, s[1].byte[0]);
              WRITE80B1F(3, s[1].byte[0]);
              WRITE80B0F(4, s[2].byte[0]);
              WRITE80B1F(5, s[2].byte[0]);
              WRITE80B0F(6, s[3].byte[0]);
              WRITE80B1F(7, s[3].byte[0]);
              d += 8, s += 4;
            }
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  画面更新 (320x200x2 color)
//
#define WRITEC320(d)                                                                           \
  m = E80SRMask[(bp1 | rp1 >> 2 | gp1 >> 4) & 3];                                              \
  d = (d & ~PACK(GVRAMC_BIT)) | (E80SRTable[(bp1 & 0x03) | (rp1 & 0x0c) | (gp1 & 0x30)] & m) | \
      (E80SRTable[(bp2 & 0x03) | (rp2 & 0x0c) | (gp2 & 0x30)] & ~m);
#define WRITEC320F(o)                                                \
  m = E80SRMask[(bp1 | rp1 >> 2 | gp1 >> 4) & 3];                    \
  *((packed*)(((uint8_t*)(dest + o)) + bpl)) = dest[o] =             \
      (dest[o] & ~PACK(GVRAMC_BIT)) |                                \
      (E80SRTable[(bp1 & 0x03) | (rp1 & 0x0c) | (gp1 & 0x30)] & m) | \
      (E80SRTable[(bp2 & 0x03) | (rp2 & 0x0c) | (gp2 & 0x30)] & ~m);
void Screen::UpdateScreen320c(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty1 = memory_->GetDirtyFlag();
  uint8_t* dirty2 = dirty1 + 0x200;
  int y;
  for (y = 0; y < 500; y += sizeof(packed)) {
    if (*(packed*)(&dirty1[y]) || *(packed*)(&dirty2[y]))
      break;
  }
  if (y < 500) {
    y /= 5;

    int begin = y * 2, end = y * 2 + 1;

    image += 4 * bpl * y;
    dirty1 += 5 * y;
    dirty2 += 5 * y;

    Memory::Quadbyte* src1 = nullptr;
    Memory::Quadbyte* src2 = nullptr;
    uint32_t dspoff;
    if (!grph_priority_) {
      src1 = memory_->GetGVRAM() + y * 80;
      src2 = memory_->GetGVRAM() + y * 80 + 0x2000;
      dspoff = port53_;
    } else {
      src1 = memory_->GetGVRAM() + y * 80 + 0x2000;
      src2 = memory_->GetGVRAM() + y * 80;
      dspoff = ((port53_ >> 1) & 2) | ((port53_ << 1) & 4);
    }
    int dm = 0;

    uint32_t bp1, rp1, gp1, bp2, rp2, gp2;
    bp1 = rp1 = gp1 = bp2 = rp2 = gp2 = 0;

    if (!full_line_) {
      for (; y < 100; ++y, image += 4 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty1, ++dirty2) {
          if (*dirty1 || *dirty2) {
            *dirty1 = *dirty2 = 0;
            end = y * 2 + 1;
            static const int tmp[5] = {0x03, 0x0c, 0x11, 0x06, 0x18};
            dm |= tmp[x];

            packed m;
            for (int j = 0; j < 16; ++j) {
              if (!(dspoff & 2)) {
                bp1 = src1->byte[0];
                rp1 = src1->byte[1] << 2;
                gp1 = src1->byte[2] << 4;
              }
              if (!(dspoff & 4)) {
                bp2 = src2->byte[0];
                rp2 = src2->byte[1] << 2;
                gp2 = src2->byte[2] << 4;
              }

              WRITEC320(dest[3]);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320(dest[2]);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320(dest[1]);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320(dest[0]);
              if (x == 2 && j == 7)
                dest = (packed*)(image + 2 * bpl);
              else
                dest += 4;
              ++src1;
              ++src2;
            }
          } else {
            src1 += 16;
            src2 += 16;
            dest = (x == 2 ? (packed*)(image + 2 * bpl) + 32 : dest + 64);
          }
        }
      }
    } else {
      for (; y < 100; ++y, image += 4 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty1, ++dirty2) {
          if (*dirty1 || *dirty2) {
            *dirty1 = *dirty2 = 0;
            end = y * 2 + 1;
            static const int tmp[5] = {0x03, 0x0c, 0x11, 0x06, 0x18};
            dm |= tmp[x];

            packed m;
            for (int j = 0; j < 16; ++j) {
              if (!(dspoff & 2)) {
                bp1 = src1->byte[0];
                rp1 = src1->byte[1] << 2;
                gp1 = src1->byte[2] << 4;
              }
              if (!(dspoff & 4)) {
                bp2 = src2->byte[0];
                rp2 = src2->byte[1] << 2;
                gp2 = src2->byte[2] << 4;
              }

              WRITEC320F(3);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320F(2);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320F(1);
              bp1 >>= 2;
              rp1 >>= 2;
              gp1 >>= 2;
              bp2 >>= 2;
              rp2 >>= 2;
              gp2 >>= 2;
              WRITEC320F(0);
              if (x == 2 && j == 7)
                dest = (packed*)(image + 2 * bpl);
              else
                dest += 4;
              ++src1;
              ++src2;
            }
          } else {
            src1 += 16;
            src2 += 16;
            dest = (x == 2 ? (packed*)(image + 2 * bpl) + 32 : dest + 64);
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  画面更新 (320x200x6 b/w)
//
#define WRITEB320(d, a) d = (d & ~PACK(GVRAMM_BIT)) | BE80Table[a & 3]

#define WRITEB320F(o, a)                                 \
  *((packed*)(((uint8_t*)(dest + o)) + bpl)) = dest[o] = \
      (dest[o] & ~PACK(GVRAMM_BIT)) | BE80Table[a & 3]

void Screen::UpdateScreen320b(uint8_t* image, int bpl, Draw::Region& region) {
  uint8_t* dirty1 = memory_->GetDirtyFlag();
  uint8_t* dirty2 = dirty1 + 0x200;
  int y;
  for (y = 0; y < 500; y += sizeof(packed)) {
    if (*(packed*)(&dirty1[y]) || *(packed*)(&dirty2[y]))
      break;
  }
  if (y < 500) {
    y /= 5;

    int begin = y * 2, end = y * 2 + 1;

    image += 4 * bpl * y;
    dirty1 += 5 * y;
    dirty2 += 5 * y;

    Memory::Quadbyte* src = memory_->GetGVRAM() + y * 80;

    Memory::Quadbyte mask1;
    Memory::Quadbyte mask2;
    mask1.byte[0] = port53_ & 2 ? 0x00 : 0xff;
    mask1.byte[1] = port53_ & 4 ? 0x00 : 0xff;
    mask1.byte[2] = port53_ & 8 ? 0x00 : 0xff;
    mask1.byte[3] = 0;
    mask2.byte[0] = port53_ & 16 ? 0x00 : 0xff;
    mask2.byte[1] = port53_ & 32 ? 0x00 : 0xff;
    mask2.byte[2] = port53_ & 64 ? 0x00 : 0xff;
    mask2.byte[3] = 0;

    int dm = 0;
    if (!full_line_) {
      for (; y < 100; ++y, image += 4 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty1, ++dirty2) {
          if (*dirty1 || *dirty2) {
            *dirty1 = *dirty2 = 0;
            end = y * 2 + 1;
            static const int tmp[5] = {0x03, 0x0c, 0x11, 0x06, 0x18};
            dm |= tmp[x];

            for (int j = 0; j < 8; ++j) {
              uint32_t s;
              s = (src[0].pack & mask1.pack) | (src[0x2000].pack & mask2.pack);
              s = (s | (s >> 8) | (s >> 16));
              WRITEB320(dest[3], s);
              s >>= 2;
              WRITEB320(dest[2], s);
              s >>= 2;
              WRITEB320(dest[1], s);
              s >>= 2;
              WRITEB320(dest[0], s);
              s = (src[1].pack & mask1.pack) | (src[0x2001].pack & mask2.pack);
              s = (s | (s >> 8) | (s >> 16));
              WRITEB320(dest[7], s);
              s >>= 2;
              WRITEB320(dest[6], s);
              s >>= 2;
              WRITEB320(dest[5], s);
              s >>= 2;
              WRITEB320(dest[4], s);
              if (x == 2 && j == 3)
                dest = (packed*)(image + 2 * bpl);
              else
                dest += 8;
              src += 2;
            }
          } else {
            src += 16;
            dest = (x == 2 ? (packed*)(image + 2 * bpl) + 32 : dest + 64);
          }
        }
      }
    } else {
      for (; y < 100; ++y, image += 4 * bpl) {
        packed* dest = (packed*)image;

        for (int x = 0; x < 5; ++x, ++dirty1, ++dirty2) {
          if (*dirty1 || *dirty2) {
            *dirty1 = *dirty2 = 0;
            end = y * 2 + 1;
            static const int tmp[5] = {0x03, 0x0c, 0x11, 0x06, 0x18};
            dm |= tmp[x];

            for (int j = 0; j < 8; ++j) {
              uint32_t s;
              s = (src[0].pack & mask1.pack) | (src[0x2000].pack & mask2.pack);
              s = (s | (s >> 8) | (s >> 16));
              WRITEB320F(3, s);
              s >>= 2;
              WRITEB320F(2, s);
              s >>= 2;
              WRITEB320F(1, s);
              s >>= 2;
              WRITEB320F(0, s);
              s = (src[1].pack & mask1.pack) | (src[0x2001].pack & mask2.pack);
              s = (s | (s >> 8) | (s >> 16));
              WRITEB320F(7, s);
              s >>= 2;
              WRITEB320F(6, s);
              s >>= 2;
              WRITEB320F(5, s);
              s >>= 2;
              WRITEB320F(4, s);
              if (x == 2 && j == 3)
                dest = (packed*)(image + 2 * bpl);
              else
                dest += 8;
              src += 2;
            }
          } else {
            src += 16;
            dest = (x == 2 ? (packed*)(image + 2 * bpl) + 32 : dest + 64);
          }
        }
      }
    }
    region.Update(RegionTable[dm * 2], 2 * begin, RegionTable[dm * 2 + 1], 2 * end + 1);
  }
}

// ---------------------------------------------------------------------------
//  Out 30
//  b1  CRT モードコントロール
//
void IOCALL Screen::Out30(uint32_t, uint32_t data) {
  //  uint32_t i = port30 ^ data;
  port30_ = data;
  crtc_->SetTextSize(!(data & 0x01));
}

// ---------------------------------------------------------------------------
//  Out 31
//  b4  color / ~b/w
//  b3  show graphic plane
//  b0  200line / ~400line
//
void IOCALL Screen::Out31(uint32_t, uint32_t data) {
  int i = port31_ ^ data;

  if (!n80mode_) {
    if (i & 0x19) {
      port31_ = data;
      display_graphics_ = (data & 8) != 0;

      if (i & 0x11) {
        color_ = (data & 0x10) != 0;
        line400_ = !(data & 0x01) && !color_;
        crtc_->SetTextMode(color_);
      }
    }
  } else {
    if (i & 0xfc) {
      port31_ = data;
      display_graphics_ = (data & 8) != 0;

      if (i & 0xf4) {
        Draw::Palette col;
        col.green = data & 0x80 ? 7 : 0;
        col.red = data & 0x40 ? 7 : 0;
        col.blue = data & 0x20 ? 7 : 0;

        if (port33_ & 0x80) {
          if (i & 0x1c)
            palette_changed_ = true;
          line320_ = (data & 0x04) != 0;
          color_ = (data & 0x10) != 0;
          crtc_->SetTextMode(color_);
          if (!color_) {
            if (i & 0xe0)
              palette_changed_ = true;
            bg_pal_ = col;
          }
          if (color_) {
            uint32_t mask53 = (line320_ ? 6 : 2);
            display_graphics_ = ((port53_ & mask53) != mask53 && (port31_ & 8) != 0);
          }
        } else {
          palette_changed_ = true;

          line320_ = (data & 0x10) != 0;
          color_ = line320_ || (data & 0x04) != 0;
          crtc_->SetTextMode(color_);

          if (!line320_) {
            pal_[0].green = pal_[0].red = pal_[0].blue = 0;
            for (int j = 1; j < 8; ++j)
              pal_[j] = col;
            if (!color_)
              bg_pal_ = col;
          } else {
            if (data & 0x04) {
              pal_[0].blue = 7;
              pal_[0].red = 0;
              pal_[0].green = 0;
              pal_[1].blue = 7;
              pal_[1].red = 7;
              pal_[1].green = 0;
              pal_[2].blue = 7;
              pal_[2].red = 0;
              pal_[2].green = 7;
            } else {
              pal_[0].blue = 0;
              pal_[0].red = 0;
              pal_[0].green = 0;
              pal_[1].blue = 0;
              pal_[1].red = 7;
              pal_[1].green = 0;
              pal_[2].blue = 0;
              pal_[2].red = 0;
              pal_[2].green = 7;
            }
            pal_[3] = col;
            for (int j = 0; j < 4; ++j)
              pal_[4 + j] = pal_[j];
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  Out 32
//  b5  パレットモード
//
void IOCALL Screen::Out32(uint32_t, uint32_t data) {
  uint32_t i = port32_ ^ data;
  if (i & 0x20) {
    port32_ = data;
    if (!color_)
      palette_changed_ = true;
  }
}

// ---------------------------------------------------------------------------
//  Out 33
//  b7  0...N/N80,     1...N80V2
//  b3  0...Text>Grph，1...Grph>Text
//  b2  0...Gr1 > Gr2，1...Gr2 > Gr1
//
void IOCALL Screen::Out33(uint32_t, uint32_t data) {
  if (n80mode_) {
    uint32_t i = port33_ ^ data;
    if (i & 0x8c) {
      port33_ = data;
      text_priority_ = (data & 0x08) != 0;
      grph_priority_ = (data & 0x04) != 0;
      palette_changed_ = true;
    }
  }
}

// ---------------------------------------------------------------------------
//  Out 52
//  バックグラウンドカラー(デジタル)の指定
//
void IOCALL Screen::Out52(uint32_t, uint32_t data) {
  if (!(port32_ & 0x20)) {
    bg_pal_.blue = (data & 0x08) ? 255 : 0;
    bg_pal_.red = (data & 0x10) ? 255 : 0;
    bg_pal_.green = (data & 0x20) ? 255 : 0;
    Log("bgpalette(d) = %6x\n", bg_pal_.green * 0x10000 + bg_pal_.red * 0x100 + bg_pal_.blue);
    if (!color_)
      palette_changed_ = true;
  }
}

// ---------------------------------------------------------------------------
//  Out 53
//  画面重ねあわせの制御
//
void IOCALL Screen::Out53(uint32_t, uint32_t data) {
  if (!n80mode_) {
    Log("show plane(53) : %c%c%c %c\n", data & 8 ? '-' : '2', data & 4 ? '-' : '1',
        data & 2 ? '-' : '0', data & 1 ? '-' : 'T');

    if ((port53_ ^ data) & (color_ ? 0x01 : 0x0f)) {
      port53_ = data;
    }
  } else if (port33_ & 0x80) {
    Log("show plane(53) : %c%c%c%c%c%c %c\n", data & 64 ? '-' : '5', data & 32 ? '-' : '4',
        data & 16 ? '-' : '3', data & 8 ? '-' : '2', data & 4 ? '-' : '1', data & 2 ? '-' : '0',
        data & 1 ? '-' : 'T');
    uint32_t mask;
    if (color_) {
      if (line320_)
        mask = 0x07;
      else
        mask = 0x03;
    } else {
      if (line320_)
        mask = 0x7f;
      else
        mask = 0x0f;
    }

    if ((port53_ ^ data) & mask) {
      mode_changed_ = true;
      if (color_) {
        uint32_t mask53 = (line320_ ? 6 : 2);
        display_graphics_ = ((data & mask53) != mask53 && (port31_ & 8) != 0);
      }
    }
    port53_ = data;  //  画面モードが変更される可能性に備え，値は常に全ビット保存
  }
}

// ---------------------------------------------------------------------------
//  Out 54
//  set palette #0 / BG Color
//
void IOCALL Screen::Out54(uint32_t, uint32_t data) {
  if (port32_ & 0x20)  // is analog palette mode ?
  {
    Draw::Palette& p = data & 0x80 ? bg_pal_ : pal_[0];

    if (data & 0x40)
      p.green = data & 7;
    else
      p.blue = data & 7, p.red = (data >> 3) & 7;

    Log("palette(a) %c = %3x\n", data & 0x80 ? 'b' : '0',
        pal_[0].green * 0x100 + pal_[0].red * 0x10 + pal_[0].blue);
  } else {
    pal_[0].green = data & 4 ? 7 : 0;
    pal_[0].red = data & 2 ? 7 : 0;
    pal_[0].blue = data & 1 ? 7 : 0;
    Log("palette(d) 0 = %.3x\n", pal_[0].green * 0x100 + pal_[0].red * 0x10 + pal_[0].blue);
  }
  palette_changed_ = true;
}

// ---------------------------------------------------------------------------
//  Out 55 - 5b
//  Set palette #1 to #7
//
void IOCALL Screen::Out55to5b(uint32_t port, uint32_t data) {
  Draw::Palette& p = pal_[port - 0x54];

  if (!n80mode_ && (port32_ & 0x20))  // is analog palette mode?
  {
    if (data & 0x40)
      p.green = data & 7;
    else
      p.blue = data & 7, p.red = (data >> 3) & 7;
  } else {
    p.green = data & 4 ? 7 : 0;
    p.red = data & 2 ? 7 : 0;
    p.blue = data & 1 ? 7 : 0;
  }

  Log("palette    %d = %.3x\n", port - 0x54, p.green * 0x100 + p.red * 0x10 + p.blue);
  palette_changed_ = true;
}

// ---------------------------------------------------------------------------
//  画面消去
//
void Screen::ClearScreen(uint8_t* image, int bpl) {
  // COLOR

  if (color_) {
    for (int y = 0; y < 400; ++y, image += bpl) {
      auto* ptr = (packed*)image;

      for (int v = 0; v < 640 / sizeof(packed) / 4; ++v, ptr += 4) {
        ptr[0] = (ptr[0] & ~PACK(GVRAMC_BIT)) | PACK(GVRAMC_CLR);
        ptr[1] = (ptr[1] & ~PACK(GVRAMC_BIT)) | PACK(GVRAMC_CLR);
        ptr[2] = (ptr[2] & ~PACK(GVRAMC_BIT)) | PACK(GVRAMC_CLR);
        ptr[3] = (ptr[3] & ~PACK(GVRAMC_BIT)) | PACK(GVRAMC_CLR);
      }
    }
  } else {
    bool maskeven = !line400_ || n80mode_;
    int d = maskeven ? 2 * bpl : bpl;

    for (int y = (maskeven ? 200 : 400); y > 0; y--, image += d) {
      int v;
      auto* ptr = (packed*)image;

      for (v = 0; v < 640 / sizeof(packed) / 4; ++v, ptr += 4) {
        ptr[0] = (ptr[0] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_ODD);
        ptr[1] = (ptr[1] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_ODD);
        ptr[2] = (ptr[2] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_ODD);
        ptr[3] = (ptr[3] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_ODD);
      }

      if (maskeven) {
        ptr = (packed*)(image + bpl);
        for (v = 0; v < 640 / sizeof(packed) / 4; ++v, ptr += 4) {
          ptr[0] = (ptr[0] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_EVEN);
          ptr[1] = (ptr[1] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_EVEN);
          ptr[2] = (ptr[2] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_EVEN);
          ptr[3] = (ptr[3] & ~PACK(GVRAMM_BITF)) | PACK(GVRAMM_EVEN);
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  設定更新
//
void Screen::ApplyConfig(const Config* config) {
  fv15k_ = config->IsFV15k();
  pex_ = palextable[(config->flags & Config::kDigitalPalette) ? 1 : 0];
  text_tp_ = (config->flags & Config::kSpecialPalette) != 0;
  bool flp = full_line_;
  full_line_ = (config->flags & Config::kFullline) != 0;
  if (full_line_ != flp)
    mode_changed_ = true;
  palette_changed_ = true;
  newmode_ = config->basicmode;
  gmask_ = (config->flag2 / Config::kMask0) & 7;
}

// ---------------------------------------------------------------------------
//  Table 作成
//

#define CHKBIT(i, j) ((1 << j) & i)
#define BIT80SR 1

void Screen::CreateTable() {
  if (BETable0[0] == -1) {
    int i;
    for (i = 0; i < (1 << sizeof(packed)); ++i) {
      int j;
      packed p = 0, q = 0, r = 0;

      for (j = 0; j < sizeof(packed); ++j) {
        bool chkbit = CHKBIT(i, j) != 0;
        p = (p << 8) | (chkbit ? GVRAM0_SET : GVRAM0_RES);
        q = (q << 8) | (chkbit ? GVRAM1_SET : GVRAM1_RES);
        r = (r << 8) | (chkbit ? GVRAM2_SET : GVRAM2_RES);
      }
      BETable0[i] = p;
      BETable1[i] = q;
      BETable2[i] = r;
    }

    for (i = 0; i < (1 << sizeof(packed)); ++i) {
      E80Table[i] = BETable0[(i & 0x05) | ((i & 0x05) << 1)] |
                    BETable1[(i & 0x0a) | ((i & 0x0a) >> 1)] | PACK(GVRAM2_RES);
    }

    for (i = 0; i < 64; ++i) {
      packed p;

      p = (i & 0x01) ? GVRAM0_SET : GVRAM0_RES;
      p |= (i & 0x04) ? GVRAM1_SET : GVRAM1_RES;
      p |= (i & 0x10) ? GVRAM2_SET : GVRAM2_RES;
      E80SRTable[i] = p << (16 * BIT80SR);
      p = (i & 0x02) ? GVRAM0_SET : GVRAM0_RES;
      p |= (i & 0x08) ? GVRAM1_SET : GVRAM1_RES;
      p |= (i & 0x20) ? GVRAM2_SET : GVRAM2_RES;
      E80SRTable[i] |= p << (16 * (1 - BIT80SR));
      E80SRTable[i] |= E80SRTable[i] << 8;
    }

    for (i = 0; i < 4; ++i) {
      E80SRMask[i] = ((i & 1) ? 0xffff : 0x0000) << (16 * BIT80SR);
      E80SRMask[i] |= ((i & 2) ? 0xffff : 0x0000) << (16 * (1 - BIT80SR));
      BE80Table[i] = ((i & 1) ? GVRAM1_SET : GVRAM1_RES) << (16 * BIT80SR);
      BE80Table[i] |= ((i & 2) ? GVRAM1_SET : GVRAM1_RES) << (16 * (1 - BIT80SR));
      BE80Table[i] |= BE80Table[i] << 8;
    }
  }
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL Screen::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL Screen::SaveStatus(uint8_t* s) {
  auto st = (Status*)s;
  st->rev = ssrev;
  st->p30 = port30_;
  st->p31 = port31_;
  st->p32 = port32_;
  st->p33 = port33_;
  st->p53 = port53_;
  st->bgpal = bg_pal_;
  for (int i = 0; i < 8; ++i)
    st->pal[i] = pal_[i];
  return true;
}

bool IFCALL Screen::LoadStatus(const uint8_t* s) {
  const auto* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;
  Out30(0x30, st->p30);
  Out31(0x31, st->p31);
  Out32(0x32, st->p32);
  Out33(0x33, st->p33);
  Out53(0x53, st->p53);
  bg_pal_ = st->bgpal;
  for (int i = 0; i < 8; ++i)
    pal_[i] = st->pal[i];
  mode_changed_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor Screen::descriptor = {nullptr, Screen::outdef};

const Device::OutFuncPtr Screen::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Screen::Reset),     static_cast<Device::OutFuncPtr>(&Screen::Out30),
    static_cast<Device::OutFuncPtr>(&Screen::Out31),     static_cast<Device::OutFuncPtr>(&Screen::Out32),
    static_cast<Device::OutFuncPtr>(&Screen::Out33),     static_cast<Device::OutFuncPtr>(&Screen::Out52),
    static_cast<Device::OutFuncPtr>(&Screen::Out53),     static_cast<Device::OutFuncPtr>(&Screen::Out54),
    static_cast<Device::OutFuncPtr>(&Screen::Out55to5b),
};
