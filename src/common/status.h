// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.h,v 1.6 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <mutex>
#include <string>
#include <vector>

class StatusDisplay {
 public:
  StatusDisplay() = default;
  virtual ~StatusDisplay() = default;

  void FDAccess(uint32_t dr, bool hd, bool active);
  virtual void UpdateDisplay() = 0;
  virtual void Update() = 0;
  virtual void WaitSubSys() { litstat_[2] = 9; }

  void Show(int priority, int duration, const char* msg, ...);

 protected:
  friend class StatusDisplay;
  struct Entry {
    int priority;
    uint64_t end_time;
    std::string msg;
    bool clear;
  };

  void ShowV(int priority, int duration, const char* msg, va_list args);
  void Clean();

  std::mutex mtx_;
  std::vector<Entry> entries_;

  int litstat_[3]{};

  uint64_t current_end_time_ = 0;
  int current_priority_ = 10000;
  bool update_message_ = false;
};

extern StatusDisplay* g_status_display;