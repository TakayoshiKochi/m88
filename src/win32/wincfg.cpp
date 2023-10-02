// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: wincfg.cpp,v 1.8 2003/05/12 22:26:35 cisc Exp $

#include "win32/wincfg.h"

#include <algorithm>

#include "win32/messages.h"
#include "win32/resource.h"

using namespace PC8801;

WinConfig* WinConfig::instance = 0;

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinConfig::WinConfig()
    : pplist_(nullptr),
      npages_(0),
      ccpu_(config_, orgconfig_),
      cscrn_(config_, orgconfig_),
      csound_(config_, orgconfig_),
      cvol_(config_, orgconfig_),
      cfunc_(config_, orgconfig_),
      cswitch_(config_, orgconfig_),
      cenv_(config_, orgconfig_),
      cromeo_(config_, orgconfig_) {
  page_ = 0;
  hwndps_ = nullptr;

  Add(&ccpu_);
  Add(&cscrn_);
  Add(&csound_);
  Add(&cvol_);
  Add(&cswitch_);
  Add(&cfunc_);
  Add(&cenv_);
  Add(&cromeo_);
  instance = this;
}

WinConfig::~WinConfig() {
  propsheets.clear();
  instance = nullptr;
}

// ---------------------------------------------------------------------------
//  設定ダイアログの実行
//
bool WinConfig::Show(HINSTANCE hinstance, HWND hwnd, Config* conf) {
  if (!hwndps_) {
    orgconfig_ = config_ = *conf;
    hinst_ = hinstance;
    hwndparent_ = hwnd;

    PROPSHEETHEADER psh;
    PROPSHEETPAGE* psp = new PROPSHEETPAGE[propsheets.size()];
    if (!psp)
      return false;

    ccpu_.Init(hinst_);
    cscrn_.Init(hinst_);
    csound_.Init(hinst_);
    cvol_.Init(hinst_);
    cfunc_.Init(hinst_);
    cswitch_.Init(hinst_);
    cenv_.Init(hinst_);
    cromeo_.Init(hinst_);

    // 拡張モジュールの場合、headerのバージョンによって PROPSHEETPAGE のサイズが違ったりする
    PROPSHEETPAGE tmppage[2];  // 2個分確保するのは、拡張側の PROPSHEETPAGE のサイズが
                               // M88 の PROPSHEETPAGE より大きいケースに備えている

    int i = 0;
    for (auto n = propsheets.begin(); n != propsheets.end() && i < MAXPROPPAGES; ++n) {
      memset(tmppage, 0, sizeof(tmppage));
      tmppage[0].dwSize = sizeof(PROPSHEETPAGE);

      if ((*n)->Setup(this, tmppage)) {
        memcpy(&psp[i], tmppage, sizeof(PROPSHEETPAGE));
        psp[i].dwSize = sizeof(PROPSHEETPAGE);
        ++i;
      }
    }

    if (i > 0) {
      memset(&psh, 0, sizeof(psh));
#if _WIN32_IE > 0x200
      psh.dwSize = PROPSHEETHEADER_V1_SIZE;
#else
      psh.dwSize = sizeof(psh);
#endif
      psh.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS;
      psh.hwndParent = hwndparent_;
      psh.hInstance = hinst_;
      psh.pszCaption = "設定";
      psh.nPages = i;
      psh.nStartPage = std::min(page_, i - 1);
      psh.ppsp = psp;
      psh.pfnCallback = (PFNPROPSHEETCALLBACK)(void*)PropProcGate;

      hwndps_ = (HWND)PropertySheet(&psh);
    }
    delete[] psp;
  } else {
    SetFocus(hwndps_);
  }
  return false;
}

void WinConfig::Close() {
  if (hwndps_) {
    DestroyWindow(hwndps_);
    hwndps_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
//  PropSheetProc
//
bool WinConfig::ProcMsg(MSG& msg) {
  if (hwndps_) {
    if (PropSheet_IsDialogMessage(hwndps_, &msg)) {
      if (!PropSheet_GetCurrentPageHwnd(hwndps_))
        Close();
      return true;
    }
  }
  return false;
}

int WinConfig::PropProc(HWND hwnd, UINT m, LPARAM) {
  switch (m) {
    case PSCB_INITIALIZED:
      hwndps_ = hwnd;
      break;
  }
  return 0;
}

int CALLBACK WinConfig::PropProcGate(HWND hwnd, UINT m, LPARAM l) {
  if (instance) {
    return instance->PropProc(hwnd, m, l);
  } else {
    return 0;
  }
}

// ---------------------------------------------------------------------------

bool IFCALL WinConfig::Add(IConfigPropSheet* sheet) {
  propsheets.push_back(sheet);
  //  propsheets.insert(propsheets.begin(), sheet);
  return true;
}

bool IFCALL WinConfig::Remove(IConfigPropSheet* sheet) {
  PropSheets::iterator i = find(propsheets.begin(), propsheets.end(), sheet);
  if (i != propsheets.end()) {
    propsheets.erase(i);
    return true;
  }
  return false;
}

bool IFCALL WinConfig::PageSelected(IConfigPropSheet* sheet) {
  PropSheets::iterator i = find(propsheets.begin(), propsheets.end(), sheet);
  if (i == propsheets.end()) {
    page_ = 0;
    return false;
  }
  page_ = i - propsheets.begin();
  return true;
}

bool IFCALL WinConfig::PageChanged(HWND hdlg) {
  if (hwndps_)
    PropSheet_Changed(hwndps_, hdlg);
  return true;
}

bool IFCALL WinConfig::Apply() {
  orgconfig_ = config_;
  PostMessage(hwndparent_, WM_M88_APPLYCONFIG, (WPARAM)&config_, 0);
  return true;
}

void IFCALL WinConfig::_ChangeVolume(bool current) {
  PostMessage(hwndparent_, WM_M88_CHANGEVOLUME, (WPARAM)(current ? &config_ : &orgconfig_), 0);
}
