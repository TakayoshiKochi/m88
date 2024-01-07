// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

#include <windows.h>

#include "common/scoped_handle.h"

#include <mutex>

namespace services {

class PowerManagement {
 public:
  ~PowerManagement() = default;

  static PowerManagement* GetInstance() {
    std::call_once(once_, &PowerManagement::Init);
    return &instance_;
  }

  // Power management
  void PreventSleep() const;
  void AllowSleep() const;

 private:
  PowerManagement() = default;
  static void Init();
  void InitInstance();

  static PowerManagement instance_;
  static std::once_flag once_;

  scoped_handle<HANDLE> hpower_;
};

}  // namespace services