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
#include "common/scheduler.h"

#include <memory>

class IOBus;
class Scheduler;

namespace PC8801 {
class Config;
class PD8257;

// ---------------------------------------------------------------------------
//  CRTC (μPD3301) 及びテキスト画面合成
//
class CRTC : public Device {
 public:
  enum IDOut { kReset = 0, kOut, kPCGOut, kSetKanaMode };
  enum IDIn {
    kIn = 0,
    kGetStatus,
  };

  explicit CRTC(const ID& id);
  ~CRTC() override = default;

  // model
  bool Init(IOBus* bus, Scheduler* sched, PD8257* dmac);

  void ApplyConfig(const Config* config);
  //  1 フレーム分に相当する時間を求める
  [[nodiscard]] int GetFramePeriod() const { return int(GetFramePeriodNS() / 10000); }
  [[nodiscard]] int GetFramePeriodNS() const { return line_time_ns_ * (height_ + v_retrace_); }

  // rendering
  void UpdateScreen(uint8_t* image, int bpl, Draw::Region& region, bool refresh);
  //  画面サイズ変更の必要があれば変更
  // void SetSize() {}

  // Implements Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override { return sizeof(Status); }
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

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
  FRIEND_TEST(CRTCTest, ExpandAttributesTest);
  FRIEND_TEST(CRTCTest, ChangeAttrTest);

  // Note: only lower 8bits are saved.
  enum Flags : uint32_t {
    kInverse = 1 << 0,  // reverse bit と同じ
    kColor = 1 << 1,
    kControl = 1 << 2,
    kSkipline = 1 << 3,
    kNonTransparent = 1 << 4,
    kAttribute = 1 << 5,
    kClear = 1 << 6,
    kRefresh = 1 << 7,
    kEnable = 1 << 8,
    kSuppressDisplay = 1 << 9,
    kResize = 1 << 10,
  };

  //  ATTR BIT 配置     G  R  B  CG UL OL SE RE
  enum TextAttr {
    kReverse = 1 << 0,
    kSecret = 1 << 1,
    kOverline = 1 << 2,
    kUnderline = 1 << 3,
    kGraphics = 1 << 4,
  };

  enum {
    kDMABank = 2,
  };

  // For state save/restore
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
    // Note: |flags_| is uint32_t.
    uint8_t flags;
    uint8_t status;
    uint8_t column;
    uint8_t attr;
    uint8_t event;
    bool color;
  };

  // model related
  void HotReset();
  uint32_t Command(bool a0, uint32_t data);
  void SetFlag(uint32_t flags) { flags_ |= flags; }
  void ResetFlag(uint32_t flags) { flags_ &= ~flags; }
  [[nodiscard]] bool IsSet(uint32_t flags) const { return (flags_ & flags) != 0; }

  // interface to font data
  bool LoadFontFile();
  void CreateTFont();
  void CreateKanaFont();
  void CreateTFontSub(const uint8_t*, int, int);
  void ModifyFont(uint32_t off, uint32_t d);
  void CreateGFont();

  // font data access
  // フォントのアドレスを取得
  [[nodiscard]] const uint8_t* GetFont(uint32_t c) const { return &font_[c * 64]; }
  // フォント(40文字)のアドレスを取得
  [[nodiscard]] const uint8_t* GetFontW(uint32_t c) const { return &font_[0x8000 + c * 128]; }
  void EnablePCG(bool);

  // rendering
  void IOCALL StartDisplay(uint32_t = 0);
  void IOCALL ExpandLine(uint32_t = 0);
  void IOCALL ExpandLineEnd(uint32_t = 0);
  int ExpandLineSub();

  void ClearText(uint8_t* image);
  void ExpandImage(uint8_t* image, Draw::Region& region);
  void ExpandAttributes(uint8_t* dest, const uint8_t* src, uint32_t y);
  [[nodiscard]] uint8_t ChangeAttr(uint8_t code, uint8_t old_attr) const;

  void DrawInline(packed& dest, packed data) { dest = (dest & pat_mask_) | data; }

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

  IOBus* bus_ = nullptr;
  PD8257* dmac_ = nullptr;
  Scheduler* scheduler_ = nullptr;
  Scheduler::Event* sev_ = nullptr;

  // state-saved parameters --------
  int cmdm_ = 0;
  int cmdc_ = 0;
  uint8_t pcount[2] = {0, 0};
  uint8_t param0_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t param1_ = 0;
  // カーソル位置
  uint32_t cursor_x_ = 0;
  uint32_t cursor_y_ = 0;
  // b0:blink, b1:underline (-1=none)
  int cursor_type = 0;
  uint8_t attr_ = 0;
  uint32_t column_ = 0;
  uint32_t flags_ = 0;
  // CRTC status (status return value)
  //  Status Bit
  //      b0      Light Pen
  //      b1      E(?)
  //      b2      N(?)
  //      b3      DMA under run
  //      b4      Video Enable
  uint32_t status_ = 0;
  uint8_t event_ = 0;
  // Note: color is saved from |pat_rev_|

  // state-unsaved parameters --------
  uint32_t cursor_mode_ = 0;
  uint32_t line_size_ = 0;
  // 15kHz mode
  // depends on I/O port 0x40 bit 1
  bool line200_ = false;
  uint8_t attr_cursor_ = 0;
  // when blinking, 0 or 1 to control visibility
  uint8_t attr_blink_ = 0;
  // Time for displaying one text line in nanoseconds
  uint64_t line_time_ns_ = 0;
  uint32_t frame_time_ = 0;
  uint32_t pcg_adr_ = 0;
  uint32_t pcg_dat_ = 0;

  int bpl_ = 1;
  packed pat_col_ = 0;
  // mask pattern
  packed pat_mask_ = 0;
  // reverse pattern
  packed pat_rev_ = 0;
  int underline_ptr_ = 0;

  // font data for rendering for text/semi-graphics/wide-text
  // 8x8 image per character (TEXT_SET or TEXT_RES per char)
  // 0x0000-0x3fff: normal font
  // 0x4000-0x7fff: semi-graphics
  // 0x8000-0x17fff: wide font (16x8)
  std::unique_ptr<uint8_t[]> font_;
  // raw font rom data, 8bytes per character * 256 characters (2048bytes total)
  std::unique_ptr<uint8_t[]> fontrom_;
  // Area for PCG font image
  std::unique_ptr<uint8_t[]> pcgram_;
  // PC-8001mkIISR CGROM
  std::unique_ptr<uint8_t[]> cg80rom_;
  std::unique_ptr<uint8_t[]> vram_;
  uint8_t* vram_ptr_[2] = {nullptr, nullptr};
  uint8_t* attrcache_ = nullptr;

  // VRAM Cache のバンク
  uint32_t bank_ = 0;

  // 1画面のテキストサイズ
  // uint32_t tvramsize_;
  // 画面の幅
  // uint32_t screen_width_;
  // 画面の高さ
  uint32_t screen_height_ = 400;

  // 1行あたりのアトリビュート数
  uint32_t attr_per_line_ = 1;
  // 1行あたりのテキスト高さ
  uint32_t line_char_limit_ = 1;
  // 1行のドット数
  uint32_t lines_per_char_ = 1;
  // テキスト画面の幅
  uint32_t width_ = 80;
  // テキスト画面の高さ
  uint32_t height_ = 25;
  // ブリンクの速度
  uint32_t blink_rate_ = 1;

  // Vertical Retrace (in lines)
  uint32_t v_retrace_ = 0;
  // Horizontal Retrace (in columns)
  uint32_t h_retrace_ = 0;

  bool widefont_ = false;
  bool pcg_enable_ = false;
  // ひらカナ選択有効
  bool kana_enable_ = false;
  // b4 = ひらがなモード
  uint8_t kana_mode_ = 0;

 private:
  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
