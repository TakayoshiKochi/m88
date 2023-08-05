// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998.
// ---------------------------------------------------------------------------
//  CRTC (μPD3301) のエミュレーション
// ---------------------------------------------------------------------------
//  $Id: crtc.h,v 1.19 2002/04/07 05:40:09 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/draw.h"
#include "common/schedule.h"

class IOBus;
class Scheduler;

namespace PC8801 {
class PD8257;
class Config;

// ---------------------------------------------------------------------------
//  CRTC (μPD3301) 及びテキスト画面合成
//
class CRTC : public Device {
 public:
  enum IDOut { reset = 0, out, pcgout, setkanamode };
  enum IDIn {
    in = 0,
    getstatus,
  };

 public:
  CRTC(const ID& id);
  ~CRTC();
  bool Init(IOBus* bus, Scheduler* s, PD8257* dmac, Draw* draw);
  const Descriptor* IFCALL GetDesc() const { return &descriptor; }

  void UpdateScreen(uint8_t* image, int bpl, Draw::Region& region, bool refresh);
  void SetSize();
  void ApplyConfig(const Config* config);
  int GetFramePeriod();

  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

  // CRTC Control
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL Out(uint32_t, uint32_t data);
  uint32_t IOCALL In(uint32_t = 0);
  uint32_t IOCALL GetStatus(uint32_t = 0);
  void IOCALL PCGOut(uint32_t, uint32_t);
  void IOCALL SetKanaMode(uint32_t, uint32_t);

  void SetTextMode(bool color);
  void SetTextSize(bool wide);

 private:
  enum Mode {
    inverse = 1 << 0,  // reverse bit と同じ
    color = 1 << 1,
    control = 1 << 2,
    skipline = 1 << 3,
    nontransparent = 1 << 4,
    attribute = 1 << 5,
    clear = 1 << 6,
    refresh = 1 << 7,
    enable = 1 << 8,
    suppressdisplay = 1 << 9,
    resize = 1 << 10,
  };

  //  ATTR BIT 配置     G  R  B  CG UL OL SE RE
  enum TextAttr {
    reverse = 1 << 0,
    secret = 1 << 1,
    overline = 1 << 2,
    underline = 1 << 3,
    graphics = 1 << 4,
  };

  enum {
    dmabank = 2,
  };

 private:
  enum {
    ssrev = 2,
  };
  struct Status {
    uint8_t rev;
    uint8_t cmdm;
    uint8_t cmdc;
    uint8_t pcount[2];
    uint8_t param0[6];
    uint8_t param1;
    uint8_t cursor_x, cursor_y;
    int8_t cursor_t;
    uint8_t mode;
    uint8_t status;
    uint8_t column;
    uint8_t attr;
    uint8_t event;
    bool color;
  };

  void HotReset();
  bool LoadFontFile();
  void CreateTFont();
  void CreateTFont(const uint8_t*, int, int);
  void CreateKanaFont();
  void CreateGFont();
  uint32_t Command(bool a0, uint32_t data);

  void IOCALL StartDisplay(uint32_t = 0);
  void IOCALL ExpandLine(uint32_t = 0);
  void IOCALL ExpandLineEnd(uint32_t = 0);
  int ExpandLineSub();

  void ClearText(uint8_t* image);
  void ExpandImage(uint8_t* image, Draw::Region& region);
  void ExpandAttributes(uint8_t* dest, const uint8_t* src, uint32_t y);
  void ChangeAttr(uint8_t code);

  const uint8_t* GetFont(uint32_t c);
  const uint8_t* GetFontW(uint32_t c);
  void ModifyFont(uint32_t off, uint32_t d);
  void EnablePCG(bool);

  void PutChar(packed* dest, uint8_t c, uint8_t a);
  void PutNormal(packed* dest, const packed* src);
  void PutReversed(packed* dest, const packed* src);
  void PutLineNormal(packed* dest, uint8_t attr);
  void PutLineReversed(packed* dest, uint8_t attr);

  void PutCharW(packed* dest, uint8_t c, uint8_t a);
  void PutNormalW(packed* dest, const packed* src);
  void PutReversedW(packed* dest, const packed* src);
  void PutLineNormalW(packed* dest, uint8_t attr);
  void PutLineReversedW(packed* dest, uint8_t attr);

  IOBus* bus;
  PD8257* dmac;
  Scheduler* scheduler;
  Scheduler::Event* sev;
  Draw* draw;

  int cmdm, cmdc;
  uint32_t cursormode;
  uint32_t linesize;
  bool line200;  // 15KHz モード
  uint8_t attr;
  uint8_t attr_cursor;
  uint8_t attr_blink;
  uint32_t status;
  uint32_t column;
  int linetime;
  uint32_t frametime;
  uint32_t pcgadr;
  uint32_t pcgdat;

  int bpl;
  packed pat_col;
  packed pat_mask;
  packed pat_rev;
  int underlineptr;

  uint8_t* fontrom;
  uint8_t* cg80rom;  // PC-8001mkIISR CGROM
  uint8_t* font;
  uint8_t* pcgram;
  uint8_t* vram[2];
  uint8_t* attrcache;

  uint32_t bank;          // VRAM Cache のバンク
                          //  uint32_t tvramsize;         // 1画面のテキストサイズ
                          //  uint32_t screenwidth;       // 画面の幅
  uint32_t screenheight;  // 画面の高さ

  uint32_t cursor_x;  // カーソル位置
  uint32_t cursor_y;
  uint32_t attrperline;    // 1行あたりのアトリビュート数
  uint32_t linecharlimit;  // 1行あたりのテキスト高さ
  uint32_t linesperchar;   // 1行のドット数
  uint32_t width;          // テキスト画面の幅
  uint32_t height;         // テキスト画面の高さ
  uint32_t blinkrate;      // ブリンクの速度
  int cursor_type;         // b0:blink, b1:underline (-1=none)
  uint32_t vretrace;       //
  uint32_t mode;
  bool widefont;
  bool pcgenable;
  bool kanaenable;   // ひらカナ選択有効
  uint8_t kanamode;  // b4 = ひらがなモード

  uint8_t pcount[2];
  uint8_t param0[6];
  uint8_t param1;
  uint8_t event;

 private:
  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];

  static const packed colorpattern[8];
};

// ---------------------------------------------------------------------------
//  1 フレーム分に相当する時間を求める
//
inline int CRTC::GetFramePeriod() {
  return linetime * (height + vretrace);
}

}  // namespace PC8801
