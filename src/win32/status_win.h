// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.h,v 1.6 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <windows.h>

#include "common/status.h"

#include <stdint.h>

#include <string>
#include <mutex>
#include <vector>

class StatusDisplayWin : public StatusDisplay {
 public:
  StatusDisplayWin();
  ~StatusDisplayWin() override;

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
  [[nodiscard]] HWND GetHWnd() const { return chwnd_; }

 private:
  struct Border {
    int horizontal;
    int vertical;
    int split;
  };

  HWND chwnd_ = nullptr;
  HWND parent_hwnd_ = nullptr;
  UINT_PTR timer_id_ = 0;

  Border border_{};
  int height_ = 0;
  bool show_fdc_status_ = false;

  int litcurrent_[3]{};

  char buf_[128]{};
};

extern StatusDisplayWin statusdisplay;