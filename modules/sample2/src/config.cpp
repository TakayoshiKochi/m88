// $Id: config.cpp,v 1.2 1999/12/28 10:33:39 cisc Exp $

#include "config.h"
#include "resource.h"

bool ConfigMP::Init(HINSTANCE hinst) {
  hinst_ = hinst;
  return true;
}

bool ConfigMP::Setup(IConfigPropBase* base, PROPSHEETPAGE* psp) {
  base_ = base;

  memset(psp, 0, sizeof(PROPSHEETPAGE));
  psp->dwSize = sizeof(PROPSHEETPAGE);
  psp->dwFlags = 0;
  psp->hInstance = hinst_;
  psp->pszTemplate = MAKEINTRESOURCE(IDD_CONFIG);
  psp->pszIcon = nullptr;
  psp->pfnDlgProc = static_cast<DLGPROC>((void*)PageGate);
  psp->lParam = reinterpret_cast<LPARAM>(this);
  return true;
}

INT_PTR ConfigMP::PageProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
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

// static
INT_PTR CALLBACK ConfigMP::PageGate(HWND hwnd, UINT m, WPARAM w, LPARAM l) {
  ConfigMP* config = nullptr;

  if (m == WM_INITDIALOG) {
    PROPSHEETPAGE* pPage = (PROPSHEETPAGE*)l;
    config = reinterpret_cast<ConfigMP*>(pPage->lParam);
    if (config) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)config);
    }
  } else {
    config = (ConfigMP*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (config) {
    return config->PageProc(hwnd, m, w, l);
  } else {
    return FALSE;
  }
}
