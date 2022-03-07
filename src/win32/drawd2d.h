// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  Direct2D による画面描画
// ---------------------------------------------------------------------------

#pragma once

#include <d2d1.h>
#include <d2d1helper.h>

#include "win32/windraw.h"

class WinDrawD2D : public WinDrawSub {
 public:
  WinDrawD2D();
  ~WinDrawD2D();

  bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID*);
  bool Resize(uint32_t width, uint32_t height);
  bool Cleanup();
  void SetPalette(PALETTEENTRY* pal, int index, int nentries);
  void SetGUIMode(bool guimode);
  void DrawScreen(const RECT& rect, bool refresh);
  bool Lock(uint8_t** pimage, int* pbpl);
  bool Unlock();

 private:
  bool CreateD2D();

  struct BI256  // BITMAPINFO
  {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
  };

  bool MakeBitmap();

  ID2D1Factory* m_D2DFact;
  ID2D1HwndRenderTarget* m_RenderTarget;
  ID2D1GdiInteropRenderTarget* m_GDIRT;

  bool m_UpdatePal;
  HWND m_hWnd;
  HWND m_hCWnd;
  uint32_t m_width;
  uint32_t m_height;
  BYTE* m_image;  // 画像Bitmap
  BI256 m_bmpinfo;
  HBITMAP m_hBitmap;
  int bpl;
};
