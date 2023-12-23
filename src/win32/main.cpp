// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: main.cpp,v 1.9 2001/02/21 11:58:55 cisc Exp $

#include <windows.h>
#include <commctrl.h>
#include <memory>

#include "win32/ui.h"
#include "win32/file.h"

// Use modern visual style in common controls
// https://learn.microsoft.com/ja-jp/windows/win32/controls/cookbook-overview
#if defined _M_IX86
#pragma comment( \
        linker,  \
            "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment( \
        linker,  \
            "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment( \
        linker,  \
            "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// ---------------------------------------------------------------------------

char m88dir[MAX_PATH];

// ---------------------------------------------------------------------------
//  InitPathInfo
//
static void InitPathInfo() {
  char buf[MAX_PATH];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];

  GetModuleFileName(0, buf, MAX_PATH);
  _splitpath(buf, drive, dir, fname, ext);
  sprintf(m88dir, "%s%s", drive, dir);
}

// ---------------------------------------------------------------------------
//  WinMain
//
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR cmdline, int nwinmode) {
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

  if (FAILED(CoInitialize(nullptr)))
    return -1;

  InitPathInfo();
  InitCommonControls();

  int r = -1;

  auto ui = std::make_unique<WinUI>(hinst);
  if (ui && ui->InitWindow(nwinmode)) {
    r = ui->Main(cmdline);
  }

  CoUninitialize();
  return r;
}
