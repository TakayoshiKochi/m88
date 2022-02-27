// ---------------------------------------------------------------------------
//  M88 - PC8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  DirectDraw によるウインドウ画面描画
// ---------------------------------------------------------------------------
//  $Id: drawddw.h,v 1.7 2002/04/07 05:40:10 cisc Exp $

#if !defined(win32_drawddw_h)
#define win32_drawddw_h

#include "win32/windraw.h"

// ---------------------------------------------------------------------------

class WinDrawDDW : public WinDrawSub {
 public:
  WinDrawDDW();
  ~WinDrawDDW();

  bool Init(HWND hwnd, uint w, uint h, GUID*);
  bool Resize(uint width, uint height);
  bool Cleanup();

  void SetPalette(PALETTEENTRY* pal, int index, int nentries);
  void QueryNewPalette();
  void DrawScreen(const RECT& rect, bool refresh);
  bool Lock(uint8_t** pimage, int* pbpl);
  bool Unlock();

 private:
  bool CreateDDPalette();
  bool CreateDD2();
  bool CreateDDSPrimary();
  bool CreateDDSWork();
  bool RestoreSurface();
  HWND hwnd;

  static void Convert8bpp(void* dest, const uint8_t* src, RECT* rect, int pitch);

  LPDIRECTDRAW2 ddraw;
  LPDIRECTDRAWPALETTE ddpal;

  LPDIRECTDRAWSURFACE ddsprimary;
  LPDIRECTDRAWSURFACE ddsscrn;
  LPDIRECTDRAWCLIPPER ddcscrn;

  LPDIRECTDRAWSURFACE ddswork;

  uint32_t redmask;
  uint32_t greenmask;
  uint32_t bluemask;
  uint8_t redshift;
  uint8_t greenshift;
  uint8_t blueshift;
  bool scrnhaspal;
  bool palchanged;
  bool locked;

  uint width;
  uint height;

  PALETTEENTRY palentry[256];
};

#endif  // !defined(win32_drawddw_h)
