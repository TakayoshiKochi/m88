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
#include "common/image_codec.h"
#include "win32/monitor/loadmon.h"
#include "win32/screen/drawd3d12.h"
#include "win32/status.h"
#include "win32/winexapi.h"

// #define LOGNAME "windraw"
#include "common/diag.h"

#define DRAW_THREAD

// ---------------------------------------------------------------------------
// 構築/消滅
//
WinDraw::WinDraw() {
  should_terminate_ = false;
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
  if (!hthread_)
    hthread_ = HANDLE(_beginthreadex(nullptr, 0, ThreadEntry, reinterpret_cast<void*>(this), 0,
                                     &draw_thread_id_));
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
  should_terminate_ = false;
  active_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  後片付
//
bool WinDraw::CleanUp() {
  if (hthread_) {
    SetThreadPriority(hthread_, THREAD_PRIORITY_NORMAL);

    int i = 300;
    do {
      should_terminate_ = true;
      ::SetEvent(hevredraw_.get());
    } while (--i > 0 && WAIT_TIMEOUT == WaitForSingleObject(hthread_, 10));

    if (!i)
      TerminateThread(hthread_, 0);

    CloseHandle(hthread_), hthread_ = 0;
  }
  hevredraw_.reset();
  drawsub_.reset();
  return true;
}

// ---------------------------------------------------------------------------
//  画面描画用スレッド
//
uint32_t WinDraw::ThreadMain() {
  drawing_ = false;
  while (!should_terminate_) {
    PaintWindow();
    WaitForSingleObject(hevredraw_.get(), 1000);
  }
  return 0;
}

uint32_t __stdcall WinDraw::ThreadEntry(LPVOID arg) {
  if (arg)
    return reinterpret_cast<WinDraw*>(arg)->ThreadMain();
  else
    return 0;
}

// ---------------------------------------------------------------------------
//  スレッドの優先順位を下げる
//
void WinDraw::SetPriorityLow(bool low) {
  if (hthread_) {
    SetThreadPriority(hthread_, low ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL);
  }
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
      //          statusdisplay.Show(100, 0, "(%.3d, %.3d)-(%.3d, %.3d)", rect.left, rect.top,
      //          rect.right-1, rect.bottom-1);
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
  width_ = width;
  height_ = height;
  if (drawsub_) {
    std::lock_guard<std::mutex> lock(mtx_);
    drawsub_->Resize(width_, height_);
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
bool WinDraw::ChangeDisplayMode(bool fullscreen, bool) {
  // 現在窓(M88)が所属するモニタの GUID を取得
  memset(&gmonitor_, 0, sizeof(gmonitor_));

  hmonitor_ = (*MonitorFromWin)(hwnd_, MONITOR_DEFAULTTOPRIMARY);
  (*DDEnumerateEx)(DDEnumCallback, reinterpret_cast<LPVOID>(this), DDENUM_ATTACHEDSECONDARYDEVICES);

  if (display_type_ == None && !drawsub_) {
    DisplayType type = D3D;
    drawsub_ = std::make_unique<WinDrawD3D12>();
    if (!drawsub_->Init(hwnd_, width_, height_, &gmonitor_)) {
      drawsub_.reset();
      return false;
    }
    display_type_ = type;
  }

  ShowCursor(!fullscreen);

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
    return (!drawing_ && active_ ? static_cast<uint32_t>(Draw::Status::kReadyToDraw) : 0) |
           (refresh_ ? static_cast<uint32_t>(Draw::Status::kShouldRefresh) : 0) |
           drawsub_->GetStatus();
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
      ShowCursor(true);
    }
  } else {
    if (!--gui_count_ && drawsub_) {
      drawsub_->SetGUIMode(false);
      ShowCursor(false);
    }
  }
}

// ---------------------------------------------------------------------------
//  画面を 640x400x4 の BMP に変換する
//  dest    変換した BMP の置き場所、置けるだけの領域が必要。
//  ret     巧くできたかどうか
//
void WinDraw::CaptureScreen() {
  if (!drawsub_)
    return;

  std::unique_ptr<uint8_t[]> src = std::make_unique<uint8_t[]>(640 * 400);
  if (!src)
    return;

  uint8_t* s = nullptr;
  int bpl = 0;
  if (drawsub_->Lock(&s, &bpl)) {
    uint8_t* d = src.get();
    for (int y = 0; y < 400; y++) {
      memcpy(d, s, 640);
      d += 640;
      s += bpl;
    }
    drawsub_->Unlock();
  }

  const std::string type("png");
  std::unique_ptr<ImageCodec> codec;
  codec.reset(ImageCodec::GetCodec(type));
  if (codec) {
    codec->Encode(src.get(), palette_);
    codec->Save(ImageCodec::GenerateFileName(type));
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
