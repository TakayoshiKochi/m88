// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: wincfg.cpp,v 1.8 2003/05/12 22:26:35 cisc Exp $

#include "win32/wincfg.h"

#include <algorithm>

#include "services/88config.h"
#include "win32/messages.h"
#include "win32/resource.h"

WinConfig* WinConfig::instance = nullptr;

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinConfig::WinConfig()
    : npages_(0),
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
  prop_sheets_.clear();
  instance = nullptr;
}

// ---------------------------------------------------------------------------
//  設定ダイアログの実行
//
bool WinConfig::Show(HINSTANCE hinstance, HWND hwnd, pc8801::Config* conf) {
  if (!hwndps_) {
    orgconfig_ = config_ = *conf;
    hinst_ = hinstance;
    hwndparent_ = hwnd;

    auto* psp = new PROPSHEETPAGE[prop_sheets_.size()];
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
    for (auto n = prop_sheets_.begin(); n != prop_sheets_.end() && i < MAXPROPPAGES; ++n) {
      memset(tmppage, 0, sizeof(tmppage));
      tmppage[0].dwSize = sizeof(PROPSHEETPAGE);

      if ((*n)->Setup(this, tmppage)) {
        memcpy(&psp[i], tmppage, sizeof(PROPSHEETPAGE));
        psp[i].dwSize = sizeof(PROPSHEETPAGE);
        ++i;
      }
    }

    if (i > 0) {
      PROPSHEETHEADER psh{};
      psh.dwSize = sizeof(psh);
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
  if (m == PSCB_INITIALIZED) {
    hwndps_ = hwnd;
  }
  return 0;
}

int CALLBACK WinConfig::PropProcGate(HWND hwnd, UINT m, LPARAM l) {
  if (instance) {
    return instance->PropProc(hwnd, m, l);
  }
  return 0;
}

// ---------------------------------------------------------------------------

bool WinConfig::Add(IConfigPropSheet* sheet) {
  prop_sheets_.push_back(sheet);
  return true;
}

bool WinConfig::Remove(IConfigPropSheet* sheet) {
  auto i = find(prop_sheets_.begin(), prop_sheets_.end(), sheet);
  if (i != prop_sheets_.end()) {
    prop_sheets_.erase(i);
    return true;
  }
  return false;
}

bool WinConfig::PageSelected(IConfigPropSheet* sheet) {
  auto i = find(prop_sheets_.begin(), prop_sheets_.end(), sheet);
  if (i == prop_sheets_.end()) {
    page_ = 0;
    return false;
  }
  page_ = i - prop_sheets_.begin();
  return true;
}

bool WinConfig::PageChanged(HWND hdlg) {
  if (hwndps_)
    PropSheet_Changed(hwndps_, hdlg);
  return true;
}

bool WinConfig::Apply() {
  orgconfig_ = config_;
  services::ConfigService::GetInstance()->UpdateConfig(config_);
  PostMessage(hwndparent_, WM_M88_APPLYCONFIG, 0, 0);
  return true;
}

void WinConfig::_ChangeVolume(bool current) {
  PostMessage(hwndparent_, WM_M88_CHANGEVOLUME, (WPARAM)(current ? &config_ : &orgconfig_), 0);
}
