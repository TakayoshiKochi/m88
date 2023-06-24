// ---------------------------------------------------------------------------
//  M88 - PC8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  DirectDraw(32/24/16bpp) による全画面描画
// ---------------------------------------------------------------------------
//  $Id: DrawDDS.h,v 1.10 2002/04/07 05:40:10 cisc Exp $

#pragma once
#include "win32/windraw.h"

// ---------------------------------------------------------------------------

class WinDrawDDS : public WinDrawSub {
 public:
  WinDrawDDS(bool force480 = true);
  ~WinDrawDDS();

  bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID* drv);
  bool Resize(uint32_t w, uint32_t h);
  bool Cleanup();

  void SetPalette(PALETTEENTRY* pal, int index, int nentries);
  void QueryNewPalette();
  void DrawScreen(const RECT& rect, bool refresh);
  bool Lock(uint8_t** pimage, int* pbpl);
  bool Unlock();
  void SetGUIMode(bool guimode);
  void Flip();
  bool SetFlipMode(bool f);

 private:
  void FillBlankArea();
  bool RestoreSurface();
  bool SetScreenMode();
  bool CreateDD2(GUID* monitor);
  bool CreateDDS();
  bool Update(LPDIRECTDRAWSURFACE, const RECT& rect);

  static HRESULT WINAPI EDMCallBack(LPDDSURFACEDESC, LPVOID);

  HWND hwnd;

  LPDIRECTDRAW2 ddraw;
  LPDIRECTDRAWSURFACE ddsscrn;
  LPDIRECTDRAWSURFACE ddsback;
  LPDIRECTDRAWCLIPPER ddcscrn;

  uint32_t lines;  // 400 or 480
  uint32_t width;
  uint32_t height;
  POINT ltc;

  uint8_t* image;

  bool palchanged;
  bool guimode;
  int m_bpp;

  PALETTEENTRY palentry[256];
};
