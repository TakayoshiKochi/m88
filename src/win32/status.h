// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.h,v 1.6 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <windows.h>

#include "common/status.h"

#include <stdint.h>

#include <mutex>

class StatusDisplayImpl : public StatusDisplayInterface {
 public:
  StatusDisplayImpl();
  ~StatusDisplayImpl() override;

  bool Init(HWND parent);
  void CleanUp();

  bool Enable(bool sfs = false);
  bool Disable();
  int GetHeight() { return height_; }
  void DrawItem(DRAWITEMSTRUCT* dis);

  // Implements StatusDisplayInterface
  void FDAccess(uint32_t dr, bool hd, bool active) override;
  void UpdateDisplay() override;
  void WaitSubSys() override { litstat_[2] = 9; }

  // Implements StatusDisplayInterface
  bool Show(int priority, int duration, const char* msg, ...) override;
  bool ShowV(int priority, int duration, const char* msg, va_list args) override;
  void Update() override;

  // TODO: clean up the code paths.
  void ResetSize();

  [[nodiscard]] UINT_PTR GetTimerID() const { return timer_id_; }
  [[nodiscard]] HWND GetHWnd() const { return chwnd_; }

 private:
  struct List {
    List* next;
    int priority;
    int duration;
    char msg[127];
    bool clear;
  };
  struct Border {
    int horizontal;
    int vertical;
    int split;
  };

  void Clean();

  HWND chwnd_ = nullptr;
  HWND parent_hwnd_ = nullptr;
  List* list_ = nullptr;
  UINT_PTR timer_id_ = 0;
  std::mutex mtx_;
  Border border_{};
  int height_ = 0;
  int litstat_[3]{};
  int litcurrent_[3]{};
  bool show_fdc_status_ = false;
  bool update_message_ = false;

  int current_duration_ = 0;
  int current_priority_ = 10000;

  char buf_[128]{};
};

extern StatusDisplayImpl statusdisplay;