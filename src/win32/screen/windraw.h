// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  画面描画関係
// ---------------------------------------------------------------------------
//  $Id: windraw.h,v 1.19 2002/04/07 05:40:11 cisc Exp $

#pragma once

#include <windows.h>

#include <stdint.h>

#include "common/draw.h"
#include "common/scoped_handle.h"
#include "common/threadable.h"

#include <atomic>
#include <memory>
#include <mutex>

// ---------------------------------------------------------------------------

class WinDrawSub {
 public:
  WinDrawSub() = default;
  virtual ~WinDrawSub() = default;

  virtual bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID* display) = 0;
  virtual bool Resize(uint32_t screen_width, uint32_t screen_height) { return false; }
  virtual bool CleanUp() = 0;
  virtual void SetPalette(Draw::Palette* pal, int index, int nentries) {}
  virtual void QueryNewPalette() {}
  virtual void DrawScreen(const RECT& rect, bool refresh) = 0;
  virtual RECT GetFullScreenRect() { return {}; }

  virtual bool Lock(uint8_t** pimage, int* pbpl) { return false; }
  virtual bool Unlock() { return true; }

  virtual void SetGUIMode(bool gui) {}
  virtual uint32_t GetStatus() { return status; }
  virtual void Flip() {}
  virtual bool SetFlipMode(bool flip) { return false; }
  virtual void WindowMoved(int cx, int cy) {}

 protected:
  uint32_t status = 0;
};

// ---------------------------------------------------------------------------

class WinDraw : public Draw, public Threadable<WinDraw> {
 public:
  WinDraw();
  ~WinDraw() override;
  bool Init0(HWND hwindow);

  // - Draw Common Interface
  bool Init(uint32_t width, uint32_t height, uint32_t bpp) override;
  bool CleanUp() override;

  bool Lock(uint8_t** pimage, int* pbpl) override;
  bool Unlock() override;

  void Resize(uint32_t width, uint32_t height) override;
  void SetPalette(uint32_t index, uint32_t nents, const Palette* pal) override;
  void DrawScreen(const Region& region) override;
  RECT GetFullScreenRect() { return drawsub_->GetFullScreenRect(); }

  const Draw::Palette* GetPalette() const { return palette_; }

  uint32_t GetStatus() override;
  void Flip() override;
  bool SetFlipMode(bool f) override;

  // - Unique Interface
  int GetDrawCount() {
    int ret = draw_count_;
    draw_count_ = 0;
    return ret;
  }
  void RequestPaint();
  void QueryNewPalette(bool background);
  void Activate(bool f) { active_ = f; }

  void SetGUIFlag(bool flag);
  bool ChangeDisplayMode(bool fullscreen);
  void Refresh() { refresh_ = 1; }
  void WindowMoved(int cx, int cy);

  void CaptureScreen(uint8_t* buf);

  void ThreadInit();
  bool ThreadLoop();

 private:
  enum DisplayType { None, D3D };
  void PaintWindow();

  static BOOL WINAPI
  DDEnumCallback(GUID FAR* guid, LPSTR desc, LPSTR name, LPVOID context, HMONITOR hm);

  DisplayType display_type_ = None;

  int pal_change_begin_ = 0x100;  // パレット変更エントリの最初
  int pal_change_end_ = -1;       // パレット変更エントリの最後
  int pal_region_begin_ = 0x100;  // 使用中パレットの最初
  int pal_region_end_ = -1;       // 使用中パレットの最後
  std::atomic<bool> drawing_;     // 画面を書き換え中
  bool draw_all_ = false;         // 画面全体を書き換える
  bool active_ = false;
  bool has_palette_ = false;  // パレットを持っている

  // TODO: use bool
  int refresh_ = 0;
  RECT draw_area_{};  // 書き換える領域
  int draw_count_ = 0;
  int gui_count_ = 0;

  int screen_width_ = 640;
  int screen_height_ = 400;

  int width_ = 640;
  int height_ = 400;

  HWND hwnd_ = nullptr;
  scoped_handle<HANDLE> hevredraw_;
  std::unique_ptr<WinDrawSub> drawsub_;

  std::mutex mtx_;
  bool locked_ = false;
  bool flipmode_ = false;

  HMONITOR hmonitor_ = nullptr;  // 探索中の hmonitor
  GUID gmonitor_{};              // hmonitor に対応する GUID

  Draw::Palette palette_[0x100]{};
};
