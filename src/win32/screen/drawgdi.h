// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  GDI による画面描画
// ---------------------------------------------------------------------------
//  $Id: DrawGDI.h,v 1.6 2001/02/21 11:58:53 cisc Exp $

#if !defined(win32_drawgdi_h)
#define win32_drawgdi_h

// ---------------------------------------------------------------------------

#include "win32/screen/windraw.h"

class WinDrawGDI : public WinDrawSub {
 public:
  WinDrawGDI();
  ~WinDrawGDI();

  bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID*);
  bool Resize(uint32_t width, uint32_t height);
  bool CleanUp();
  void SetPalette(PALETTEENTRY* pal, int index, int nentries);
  void DrawScreen(const RECT& rect, bool refresh);
  bool Lock(uint8_t** pimage, int* pbpl);
  bool Unlock();

 private:
  struct BI256  // BITMAPINFO
  {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
  };

 private:
  bool MakeBitmap();

  HBITMAP hbitmap;
  uint8_t* bitmapimage;
  HWND hwnd;
  uint8_t* image;
  int bpl;
  uint32_t width;
  uint32_t height;
  bool updatepal;
  BI256 binfo;
};

#endif  // !defined(win32_drawgdi_h)
