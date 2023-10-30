// $Id: config.cpp,v 1.1 1999/12/28 11:25:53 cisc Exp $

#include "config.h"

#include "resource.h"

ConfigCDIF::ConfigCDIF() = default;

bool ConfigCDIF::Init(HINSTANCE hinst) {
  hinst_ = hinst;
  return true;
}

bool ConfigCDIF::Setup(IConfigPropBase* _base, PROPSHEETPAGE* psp) {
  base_ = _base;

  memset(psp, 0, sizeof(PROPSHEETPAGE));
  psp->dwSize = sizeof(PROPSHEETPAGE);
  psp->dwFlags = 0;
  psp->hInstance = hinst_;
  psp->pszTemplate = MAKEINTRESOURCE(IDD_CONFIG);
  psp->pszIcon = nullptr;
  psp->pfnDlgProc = (DLGPROC)(void*)PageGate;
  psp->lParam = (LPARAM)this;
  return true;
}

INT_PTR ConfigCDIF::PageProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_INITDIALOG:
      return TRUE;

    case WM_NOTIFY:
      switch (((NMHDR*)lp)->code) {
        case PSN_SETACTIVE:
          base_->PageSelected(this);
          break;

        case PSN_APPLY:
          base_->Apply();
          return PSNRET_NOERROR;
      }
      return TRUE;
  }
  return FALSE;
}

INT_PTR CALLBACK ConfigCDIF::PageGate(HWND hwnd, UINT m, WPARAM w, LPARAM l) {
  ConfigCDIF* config = 0;

  if (m == WM_INITDIALOG) {
    PROPSHEETPAGE* pPage = (PROPSHEETPAGE*)l;
    config = reinterpret_cast<ConfigCDIF*>(pPage->lParam);
    if (config) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)config);
    }
  } else {
    config = (ConfigCDIF*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (config) {
    return config->PageProc(hwnd, m, w, l);
  } else {
    return FALSE;
  }
}
