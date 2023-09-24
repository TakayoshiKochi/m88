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
#include "win32/screen/drawd2d.h"
#include "win32/screen/drawd3d12.h"
#include "win32/screen/drawgdi.h"
#include "win32/screen/drawdds.h"
#include "win32/status.h"
#include "win32/winexapi.h"

#define LOGNAME "windraw"
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
  hevredraw_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  HDC hdc = GetDC(hwnd_);
  has_palette_ = !!(GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE);
  ReleaseDC(hwnd_, hdc);

#ifdef DRAW_THREAD
  if (!hthread_)
    hthread_ = HANDLE(
        _beginthreadex(nullptr, 0, ThreadEntry, reinterpret_cast<void*>(this), 0, &draw_thread_id_));
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
      ::SetEvent(hevredraw_);
    } while (--i > 0 && WAIT_TIMEOUT == WaitForSingleObject(hthread_, 10));

    if (!i)
      TerminateThread(hthread_, 0);

    CloseHandle(hthread_), hthread_ = 0;
  }
  if (hevredraw_)
    CloseHandle(hevredraw_), hevredraw_ = 0;

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
    WaitForSingleObject(hevredraw_, 1000);
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
  ::SetEvent(hevredraw_);
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
    ::SetEvent(hevredraw_);
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
void WinDraw::Resize(uint32_t w, uint32_t h) {
  //  statusdisplay.Show(50, 2500, "Resize (%d, %d)", width, height);
  width_ = w;
  height_ = h;
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
bool WinDraw::ChangeDisplayMode(bool fullscreen, bool force480) {
  DisplayType type = fullscreen ? DDFull : D3D;

  // 現在窓(M88)が所属するモニタの GUID を取得
  memset(&gmonitor_, 0, sizeof(gmonitor_));

  hmonitor_ = (*MonitorFromWin)(hwnd_, MONITOR_DEFAULTTOPRIMARY);
  (*DDEnumerateEx)(DDEnumCallback, reinterpret_cast<LPVOID>(this), DDENUM_ATTACHEDSECONDARYDEVICES);

  if (type == display_type_)
    return false;

  bool result = true;
  // 今までのドライバを廃棄
  if (drawsub_)
    drawsub_->SetGUIMode(true);

  {
    std::lock_guard<std::mutex> lock(mtx_);
    WinDrawSub* newdraw = nullptr;

    // 新しいドライバの用意
    switch (type) {
      case GDI:
      default:
        newdraw = new WinDrawGDI();
        break;
      case DDFull:
        newdraw = new WinDrawDDS(force480);
        break;
      case D2D:
        newdraw = new WinDrawD2D();
        break;
      case D3D:
        newdraw = new WinDrawD3D12();
        break;
    }

    if (!newdraw || !newdraw->Init(hwnd_, width_, height_, &gmonitor_)) {
      // 初期化に失敗した場合 GDI ドライバで再挑戦
      if (type == DDFull)
        statusdisplay.Show(50, 2500, "画面切り替えに失敗しました");
      else
        statusdisplay.Show(120, 3000, "GDI ドライバを使用します");

      newdraw = new WinDrawGDI();
      type = GDI;
      result = false;

      if (!newdraw || !newdraw->Init(hwnd_, width_, height_, nullptr)) {
        drawsub_.reset();
        display_type_ = None;
      }
    }

    if (newdraw) {
      drawsub_.reset(newdraw);
      drawsub_->SetFlipMode(flipmode_);
      drawsub_->SetGUIMode(false);
    }

    gui_count_ = 0;
  }

  draw_all_ = true;
  refresh_ = true;
  pal_region_begin_ = std::min(pal_change_begin_, pal_region_begin_);
  pal_region_end_ = std::max(pal_change_end_, pal_region_end_);
  //      if (draw)
  //          draw->Resize(width, height);

  if (type == DDFull)
    ShowCursor(false);
  if (display_type_ == DDFull)
    ShowCursor(true);
  display_type_ = type;
  refresh_ = 2;

  return result;
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
int WinDraw::CaptureScreen(uint8_t* dest) {
  const bool half = false;
  if (!drawsub_)
    return false;

  uint8_t* src = new uint8_t[640 * 400];
  if (!src)
    return false;

  uint8_t* s;
  int bpl;
  if (drawsub_->Lock(&s, &bpl)) {
    uint8_t* d = src;
    for (int y = 0; y < 400; y++) {
      memcpy(d, s, 640);
      d += 640, s += bpl;
    }
    drawsub_->Unlock();
  }

  // 構造体の準備

  auto* filehdr = reinterpret_cast<BITMAPFILEHEADER*>(dest);
  auto* binfo = reinterpret_cast<BITMAPINFO*>(filehdr + 1);

  // headers
  ((char*)&filehdr->bfType)[0] = 'B';
  ((char*)&filehdr->bfType)[1] = 'M';
  filehdr->bfReserved1 = 0;
  filehdr->bfReserved2 = 0;
  binfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  binfo->bmiHeader.biWidth = 640;
  binfo->bmiHeader.biHeight = half ? 200 : 400;
  binfo->bmiHeader.biPlanes = 1;
  binfo->bmiHeader.biBitCount = 4;
  binfo->bmiHeader.biCompression = BI_RGB;
  binfo->bmiHeader.biSizeImage = 0;
  binfo->bmiHeader.biXPelsPerMeter = 0;
  binfo->bmiHeader.biYPelsPerMeter = 0;

  // １６色パレットの作成
  RGBQUAD* pal = binfo->bmiColors;
  memset(pal, 0, sizeof(RGBQUAD) * 16);

  uint8_t ctable[256];
  memset(ctable, 0, sizeof(ctable));

  int colors = 0;
  for (int index = 0; index < 144; index++) {
    RGBQUAD rgb;
    rgb.rgbBlue = palette_[0x40 + index].peBlue;
    rgb.rgbRed = palette_[0x40 + index].peRed;
    rgb.rgbGreen = palette_[0x40 + index].peGreen;
    //      Log("c[%.2x] = G:%.2x R:%.2x B:%.2x\n", index, rgb.rgbGreen, rgb.rgbRed, rgb.rgbBlue);
    uint32_t entry = *((uint32_t*)&rgb);

    int k;
    for (k = 0; k < colors; k++) {
      if (!((*((uint32_t*)&pal[k]) ^ entry) & 0xffffff))
        goto match;
    }
    if (colors < 15) {
      //          Log("pal[%.2x] = G:%.2x R:%.2x B:%.2x\n", colors, rgb.rgbGreen, rgb.rgbRed,
      //          rgb.rgbBlue);
      pal[colors++] = rgb;
    } else
      k = 15;
  match:
    ctable[64 + index] = k;
  }

  binfo->bmiHeader.biClrImportant = colors;

  colors = 16;  // やっぱ固定じゃなきゃ駄目か？
  uint8_t* image = ((uint8_t*)(binfo + 1)) + (colors - 1) * sizeof(RGBQUAD);
  filehdr->bfSize = image + 640 * 400 / 2 - dest;
  binfo->bmiHeader.biClrUsed = colors;
  filehdr->bfOffBits = image - dest;

  // 色変換
  uint8_t* d = image;
  for (int y = 0; y < 400; y += half ? 2 : 1) {
    uint8_t* s = src + 640 * (399 - y);

    for (int x = 0; x < 320; x++, s += 2)
      *d++ = ctable[s[0]] * 16 + ctable[s[1]];
  }

  delete[] src;
  return d - dest;
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
