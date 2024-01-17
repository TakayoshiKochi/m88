// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  画面関係 for Windows
// ---------------------------------------------------------------------------
//  $Id: windraw.cpp,v 1.34 2003/04/22 13:16:36 cisc Exp $

#include "win32/screen/windraw.h"

#include <assert.h>
#include <ddraw.h>

#include <algorithm>

#include "common/error.h"
#include "win32/monitor/loadmon.h"
#include "win32/screen/drawd3d12.h"
#include "win32/status_bar_win.h"
#include "win32/winexapi.h"

// #define LOGNAME "windraw"
#include "common/diag.h"

#define DRAW_THREAD

// ---------------------------------------------------------------------------
// 構築/消滅
//
WinDraw::WinDraw() {
  drawing_ = false;
}

WinDraw::~WinDraw() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WinDraw::Init0(HWND hwindow) {
  hwnd_ = hwindow;
  drawsub_.reset();
  display_type_ = None;

  hthread_ = nullptr;
  hevredraw_.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));

  HDC hdc = GetDC(hwnd_);
  has_palette_ = !!(GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE);
  ReleaseDC(hwnd_, hdc);

#ifdef DRAW_THREAD
  StartThread();
  if (!hthread_) {
    Error::SetError(Error::ThreadInitFailed);
    return false;
  }
#endif

  pal_change_begin_ = pal_region_begin_ = 0x100;
  pal_change_end_ = pal_region_end_ = -1;

  return true;
}

bool WinDraw::Init(uint32_t width, uint32_t height, uint32_t /*bpp*/) {
  width_ = width;
  height_ = height;
  active_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  後片付
//
bool WinDraw::CleanUp() {
  if (hthread_) {
    SetThreadPriority(hthread_, THREAD_PRIORITY_NORMAL);
    ::SetEvent(hevredraw_.get());
    RequestThreadStop();
  }
  hevredraw_.reset();
  drawsub_.reset();
  return true;
}

// ---------------------------------------------------------------------------
//  画面描画用スレッド
//
void WinDraw::ThreadInit() {
  SetName(L"M88 WinDraw thread");
  WaitForSingleObject(hevredraw_.get(), 1000);
  drawing_ = false;
}

bool WinDraw::ThreadLoop() {
  PaintWindow();
  WaitForSingleObject(hevredraw_.get(), 1000);
  return true;
}

// ---------------------------------------------------------------------------
//  パレット反映
//
void WinDraw::QueryNewPalette(bool /*bkgnd*/) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (drawsub_)
    drawsub_->QueryNewPalette();
}

// ---------------------------------------------------------------------------
//  ペイントさせる
//
void WinDraw::RequestPaint() {
#ifdef DRAW_THREAD
  std::lock_guard<std::mutex> lock(mtx_);
  draw_all_ = true;
  drawing_ = true;
  ::SetEvent(hevredraw_.get());
#else
  draw_all_ = true;
  drawing_ = true;
  PaintWindow();
#endif
  //  Log("Request at %d\n", GetTickCount());
}

// ---------------------------------------------------------------------------
//  更新
//
void WinDraw::DrawScreen(const Region& region) {
  if (!drawing_) {
    Log("Draw %d to %d\n", region.top, region.bottom);
    drawing_ = true;
    draw_area_.left = std::max(0, region.left);
    draw_area_.top = std::max(0, region.top);
    draw_area_.right = std::min(width_, region.right);
    draw_area_.bottom = std::min(height_, region.bottom + 1);
#ifdef DRAW_THREAD
    ::SetEvent(hevredraw_.get());
#else
    PaintWindow();
#endif
  }
}

// ---------------------------------------------------------------------------
//  描画する
//
void WinDraw::PaintWindow() {
  LOADBEGIN("WinDraw");
  std::lock_guard<std::mutex> lock(mtx_);
  if (drawing_ && drawsub_ && active_) {
    RECT rect = draw_area_;
    if (pal_change_begin_ <= pal_change_end_) {
      pal_region_begin_ = std::min(pal_change_begin_, pal_region_begin_);
      pal_region_end_ = std::max(pal_change_end_, pal_region_end_);
      drawsub_->SetPalette(palette_ + pal_change_begin_, pal_change_begin_,
                           pal_change_end_ - pal_change_begin_ + 1);
      pal_change_begin_ = 0x100;
      pal_change_end_ = -1;
    }
    drawsub_->DrawScreen(rect, draw_all_);
    draw_all_ = false;
    if (rect.left < rect.right && rect.top < rect.bottom) {
      draw_count_++;
      Log("\t\t\t(%3d,%3d)-(%3d,%3d)\n", rect.left, rect.top, rect.right - 1, rect.bottom - 1);
      // statusdisplay.Show(100, 0, "(%.3d, %.3d)-(%.3d, %.3d)", rect.left, rect.top,
      //                    rect.right-1, rect.bottom-1);
    }
    drawing_ = false;
  }
  LOADEND("WinDraw");
}

// ---------------------------------------------------------------------------
//  パレットをセット
//
void WinDraw::SetPalette(uint32_t index, uint32_t nents, const Palette* pal) {
  assert(0 <= index && index <= 0xff);
  assert(index + nents <= 0x100);
  assert(pal);

  memcpy(palette_ + index, pal, nents * sizeof(Palette));
  pal_change_begin_ = std::min(pal_change_begin_, (int)index);
  pal_change_end_ = std::max(pal_change_end_, (int)index + (int)nents - 1);
}

// ---------------------------------------------------------------------------
//  Lock
//
bool WinDraw::Lock(uint8_t** pimage, int* pbpl) {
  assert(pimage && pbpl);

  if (!locked_) {
    locked_ = true;
    mtx_.lock();
    if (drawsub_ && drawsub_->Lock(pimage, pbpl)) {
      // Lock に失敗することがある？
      assert(**pimage >= 0);
      return true;
    }
    mtx_.unlock();
    locked_ = false;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  unlock
//
bool WinDraw::Unlock() {
  bool result = false;
  if (locked_) {
    if (drawsub_)
      result = drawsub_->Unlock();
    mtx_.unlock();
    locked_ = false;
    if (refresh_ == 1)
      refresh_ = 0;
  }
  return result;
}

// ---------------------------------------------------------------------------
//  画面サイズを変える
//
void WinDraw::Resize(uint32_t width, uint32_t height) {
  //  statusdisplay.Show(50, 2500, "Resize (%d, %d)", width, height);
  screen_width_ = width;
  screen_height_ = height;
  if (drawsub_) {
    std::lock_guard<std::mutex> lock(mtx_);
    drawsub_->Resize(screen_width_, screen_height_);
  }
}

// ---------------------------------------------------------------------------
//  画面位置を変える
//
void WinDraw::WindowMoved(int x, int y) {
  if (drawsub_)
    drawsub_->WindowMoved(x, y);
}

// ---------------------------------------------------------------------------
//  画面描画ドライバの変更
//
bool WinDraw::ChangeDisplayMode(bool fullscreen) {
  // 現在窓(M88)が所属するモニタの GUID を取得
  memset(&gmonitor_, 0, sizeof(gmonitor_));

  hmonitor_ = (*MonitorFromWin)(hwnd_, MONITOR_DEFAULTTOPRIMARY);
  (*DDEnumerateEx)(DDEnumCallback, reinterpret_cast<LPVOID>(this), DDENUM_ATTACHEDSECONDARYDEVICES);

  if (display_type_ == None && !drawsub_) {
    drawsub_ = std::make_unique<WinDrawD3D12>();
    if (!drawsub_->Init(hwnd_, width_, height_, &gmonitor_)) {
      drawsub_.reset();
      return false;
    }
    display_type_ = D3D;
  }

  draw_all_ = true;
  refresh_ = true;
  pal_region_begin_ = std::min(pal_change_begin_, pal_region_begin_);
  pal_region_end_ = std::max(pal_change_end_, pal_region_end_);
  refresh_ = 2;

  return true;
}

// ---------------------------------------------------------------------------
//  現在の状態を得る
//
uint32_t WinDraw::GetStatus() {
  if (drawsub_) {
    if (refresh_)
      refresh_ = 1;
    return (!drawing_ && active_ ? Draw::Status::kReadyToDraw : 0) |
           (refresh_ ? Draw::Status::kShouldRefresh : 0) | drawsub_->GetStatus();
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  flip モードの設定
//
bool WinDraw::SetFlipMode(bool f) {
  flipmode_ = f;
  if (drawsub_)
    return drawsub_->SetFlipMode(flipmode_);
  return false;
}

// ---------------------------------------------------------------------------
//  flip する
//
void WinDraw::Flip() {
  if (drawsub_)
    drawsub_->Flip();
}

// ---------------------------------------------------------------------------
//  GUI 使用フラグ設定
//
void WinDraw::SetGUIFlag(bool usegui) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (usegui) {
    if (!gui_count_++ && drawsub_) {
      drawsub_->SetGUIMode(true);
    }
  } else {
    if (!--gui_count_ && drawsub_) {
      drawsub_->SetGUIMode(false);
    }
  }
}

void WinDraw::CaptureScreen(uint8_t* buf) {
  if (!drawsub_)
    return;

  uint8_t* s = nullptr;
  int bpl = 0;
  if (drawsub_->Lock(&s, &bpl)) {
    uint8_t* d = buf;
    for (int y = 0; y < 400; y++) {
      memcpy(d, s, 640);
      d += 640;
      s += bpl;
    }
    drawsub_->Unlock();
  }
}

BOOL WINAPI
WinDraw::DDEnumCallback(GUID FAR* guid, LPSTR desc, LPSTR name, LPVOID context, HMONITOR hm) {
  WinDraw* draw = reinterpret_cast<WinDraw*>(context);
  if (hm == draw->hmonitor_) {
    draw->gmonitor_ = *guid;
    return 0;
  }
  return 1;
}
