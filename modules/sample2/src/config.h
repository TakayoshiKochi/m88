// $Id: config.h,v 1.2 1999/12/28 10:33:39 cisc Exp $

#pragma once

#include "if/ifcommon.h"

class ConfigMP : public IConfigPropSheet {
 public:
  ConfigMP() = default;
  bool Init(HINSTANCE hinst);
  bool IFCALL Setup(IConfigPropBase*, PROPSHEETPAGE* psp);

 private:
  INT_PTR PageProc(HWND, UINT, WPARAM, LPARAM);
  static INT_PTR CALLBACK PageGate(HWND, UINT, WPARAM, LPARAM);

  HINSTANCE hinst_ = nullptr;
  IConfigPropBase* base_ = nullptr;
};
