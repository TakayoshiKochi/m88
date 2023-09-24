// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  Direct2D による画面描画
// ---------------------------------------------------------------------------

#pragma once

#include <d2d1.h>
#include <d2d1helper.h>

#include "win32/screen/windraw.h"

class WinDrawD2D : public WinDrawSub {
 public:
  WinDrawD2D() = default;
  ~WinDrawD2D() override;

  bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID*) override;
  bool Resize(uint32_t width, uint32_t height) override;
  bool CleanUp() override;
  void SetPalette(PALETTEENTRY* pal, int index, int nentries) override;
  void SetGUIMode(bool guimode) override;
  void DrawScreen(const RECT& rect, bool refresh) override;

  bool Lock(uint8_t** pimage, int* pbpl) override;
  bool Unlock() override;

 private:
  bool CreateD2D();

  struct BI256  // BITMAPINFO
  {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
  };

  bool MakeBitmap();

  ID2D1Factory* m_D2DFact = nullptr;
  ID2D1HwndRenderTarget* m_RenderTarget = nullptr;
  ID2D1GdiInteropRenderTarget* m_GDIRT = nullptr;

  bool m_UpdatePal = false;
  HWND m_hWnd = nullptr;
  HWND m_hCWnd = nullptr;
  uint32_t m_width{};
  uint32_t m_height{};
  BYTE* m_image = nullptr;  // 画像Bitmap
  BI256 m_bmpinfo{};
  HBITMAP m_hBitmap = nullptr;
  int bpl = 0;
};
