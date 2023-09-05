// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.h,v 1.6 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <stdarg.h>
#include <stdint.h>

class StatusDisplayInterface {
 public:
  virtual ~StatusDisplayInterface() = default;

  virtual void FDAccess(uint32_t dr, bool hd, bool active) = 0;
  virtual void UpdateDisplay() = 0;
  virtual bool Show(int priority, int duration, const char* msg, ...) = 0;
  virtual void Update() = 0;
  virtual void WaitSubSys() = 0;
};

class StatusDisplay {
 public:
  StatusDisplay(StatusDisplayInterface* impl) : impl_(impl) {}
  ~StatusDisplay() = default;

  void FDAccess(uint32_t dr, bool hd, bool active) { impl_->FDAccess(dr, hd, active); }
  void UpdateDisplay() { impl_->UpdateDisplay(); }
  bool Show(int priority, int duration, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    bool r = impl_->Show(priority, duration, msg, args);
    va_end(args);
    return r;
  }
  void Update() { impl_->Update(); }
  void WaitSubSys() { impl_->WaitSubSys(); }

 private:
  StatusDisplayInterface* impl_ = nullptr;
};

// For tests
class DummyStatusDisplay : public StatusDisplayInterface {
 public:
  void FDAccess(uint32_t dr, bool hd, bool active) override {}
  void UpdateDisplay() override {}
  bool Show(int priority, int duration, const char* msg, ...) override { return true; }
  void Update() override {}
  void WaitSubSys() override {}
};

extern StatusDisplay* g_status_display;