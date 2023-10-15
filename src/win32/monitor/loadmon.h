// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: loadmon.h,v 1.1 2001/02/21 11:58:54 cisc Exp $

#pragma once

#include "common/device.h"
#include "win32/monitor/winmon.h"

#include <map>
#include <mutex>
#include <string>

class LoadMonitor : public WinMonitor {
 public:
  enum {
    presis = 10,
  };

  LoadMonitor();
  ~LoadMonitor() override;

  bool Init();

  void ProcBegin(const char* name);
  void ProcEnd(const char* name);
  static LoadMonitor* GetInstance() { return instance; }

 private:
  struct State {
    int total[presis];   // 累計
    DWORD time_entered;  // 開始時刻
  };

  typedef std::map<std::string, State> States;

  void UpdateText() override;
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM) override;
  void DrawMain(HDC, bool) override;

  States states;
  int base_ = 0;
  int tidx_ = 0;
  int tprv_ = 0;

  std::mutex mtx_;
  static LoadMonitor* instance;
};

inline void LOADBEGIN(const char* key) {
  LoadMonitor* lm = LoadMonitor::GetInstance();
  if (lm)
    lm->ProcBegin(key);
}

inline void LOADEND(const char* key) {
  LoadMonitor* lm = LoadMonitor::GetInstance();
  if (lm)
    lm->ProcEnd(key);
}
