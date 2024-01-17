// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.h,v 1.6 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <windows.h>

#include "common/status_bar.h"

#include <stdint.h>

#include <string>
#include <mutex>
#include <vector>

class StatusBarWin : public StatusBar {
 public:
  StatusBarWin();
  ~StatusBarWin() override;

  bool Init(HWND parent);
  void CleanUp();

  bool Enable(bool show_fdc_status = false);
  bool Disable();
  [[nodiscard]] int GetHeight() const { return height_; }
  void DrawItem(DRAWITEMSTRUCT* dis);

  // Implements StatusDisplay
  void UpdateDisplay();
  void Update();

  // TODO: clean up the code paths.
  void ResetSize();

  [[nodiscard]] UINT_PTR GetTimerID() const { return timer_id_; }
  [[nodiscard]] HWND GetHWnd() const { return child_hwnd_; }

 private:
  HWND parent_hwnd_ = nullptr;
  HWND child_hwnd_ = nullptr;
  UINT_PTR timer_id_ = 0;

  int height_ = 0;
  int dpi_ = 96;
  bool show_fdc_status_ = false;

  int litcurrent_[3]{};

  char buf_[128]{};
};

extern StatusBarWin statusdisplay;