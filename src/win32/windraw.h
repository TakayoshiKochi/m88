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

#include <mutex>

// ---------------------------------------------------------------------------

class WinDrawSub {
 public:
  WinDrawSub() = default;
  virtual ~WinDrawSub() = default;

  virtual bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID* display) = 0;
  virtual bool Resize(uint32_t width, uint32_t height) { return false; }
  virtual bool CleanUp() = 0;
  virtual void SetPalette(PALETTEENTRY* pal, int index, int nentries) {}
  virtual void QueryNewPalette() {}
  virtual void DrawScreen(const RECT& rect, bool refresh) = 0;

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

class WinDraw : public Draw {
 public:
  WinDraw();
  ~WinDraw() override;
  bool Init0(HWND hwindow);

  // - Draw Common Interface
  bool Init(uint32_t w, uint32_t h, uint32_t bpp) override;
  bool CleanUp() override;

  bool Lock(uint8_t** pimage, int* pbpl) override;
  bool Unlock() override;

  void Resize(uint32_t width, uint32_t height) override;
  void SetPalette(uint32_t index, uint32_t nents, const Palette* pal) override;
  void DrawScreen(const Region& region) override;

  uint32_t GetStatus() override;
  void Flip() override;
  bool SetFlipMode(bool f) override;

  // - Unique Interface
  int GetDrawCount() {
    int ret = drawcount;
    drawcount = 0;
    return ret;
  }
  void RequestPaint();
  void QueryNewPalette(bool background);
  void Activate(bool f) { active = f; }

  void SetPriorityLow(bool low);
  void SetGUIFlag(bool flag);
  bool ChangeDisplayMode(bool fullscreen, bool force480 = true);
  void Refresh() { refresh = 1; }
  void WindowMoved(int cx, int cy);

  int CaptureScreen(uint8_t* dest);

 private:
  enum DisplayType { None, GDI, DDFull, D2D, D3D };
  void PaintWindow();

  static BOOL WINAPI
  DDEnumCallback(GUID FAR* guid, LPSTR desc, LPSTR name, LPVOID context, HMONITOR hm);

  uint32_t ThreadMain();
  static uint32_t __stdcall ThreadEntry(LPVOID arg);

  uint32_t idthread;
  HANDLE hthread;
  volatile bool shouldterminate;  // スレッド終了要求

  DisplayType drawtype;

  int palcngbegin;        // パレット変更エントリの最初
  int palcngend;          // パレット変更エントリの最後
  int palrgnbegin;        // 使用中パレットの最初
  int palrgnend;          // 使用中パレットの最後
  volatile bool drawing;  // 画面を書き換え中
  bool drawall;           // 画面全体を書き換える
  bool active;
  bool haspalette;  // パレットを持っている

  int refresh;
  RECT drawarea;  // 書き換える領域
  int drawcount;
  int guicount;

  int width;
  int height;

  HWND hwnd = 0;
  HANDLE hevredraw = 0;
  WinDrawSub* drawsub_ = nullptr;

  std::mutex mtx_;
  bool locked_;
  bool flipmode_;

  HMONITOR hmonitor = 0;  // 探索中の hmonitor
  GUID gmonitor;          // hmonitor に対応する GUID

  PALETTEENTRY palette[0x100];
};
