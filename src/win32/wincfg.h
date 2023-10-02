// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: wincfg.h,v 1.4 2003/05/12 22:26:35 cisc Exp $

#pragma once

#include <vector>

#include "if/ifcommon.h"
#include "pc88/config.h"
#include "win32/cfgpage.h"

namespace PC8801 {

class WinConfig : public IConfigPropBase {
 public:
  WinConfig();
  ~WinConfig();

  bool Show(HINSTANCE, HWND, Config* config);
  bool ProcMsg(MSG& msg);
  bool IsOpen() { return hwndps_ != nullptr; }
  void Close();

 private:
  struct PPNode {
    PPNode* next;
    IConfigPropSheet* sheet;
  };

  bool IFCALL Add(IConfigPropSheet* sheet) override;
  bool IFCALL Remove(IConfigPropSheet* sheet) override;

  bool IFCALL Apply() override;
  bool IFCALL PageSelected(IConfigPropSheet*) override;
  bool IFCALL PageChanged(HWND) override;
  void IFCALL _ChangeVolume(bool) override;

  int PropProc(HWND, UINT, LPARAM);
  static int CALLBACK PropProcGate(HWND, UINT, LPARAM);
  static WinConfig* instance;

  PPNode* pplist_ = nullptr;
  typedef std::vector<IConfigPropSheet*> PropSheets;
  PropSheets propsheets;

  HWND hwndparent_ = nullptr;
  HWND hwndps_ = nullptr;
  HINSTANCE hinst_ = nullptr;
  Config config_;
  Config orgconfig_;
  int page_;
  int npages_;

  ConfigCPU ccpu_;
  ConfigScreen cscrn_;
  ConfigSound csound_;
  ConfigVolume cvol_;
  ConfigFunction cfunc_;
  ConfigSwitch cswitch_;
  ConfigEnv cenv_;
  ConfigROMEO cromeo_;
};

}  // namespace PC8801
