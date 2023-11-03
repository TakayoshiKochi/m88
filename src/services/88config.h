// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: 88config.h,v 1.9 2000/08/06 09:59:20 cisc Exp $

#pragma once

#include "if/ifcommon.h"
#include "pc88/config.h"

#include <mutex>
#include <string_view>

namespace services {

class ConfigService {
 public:
  static ConfigService* GetInstance() {
    std::call_once(once_, &ConfigService::Init);
    return &instance_;
  }

  void Save();
  void LoadConfigDirectory(const char* entry, bool readalways);
  // Save new config, and then post WM_M88_APPLYCONFIG message to main window.
  void UpdateConfig(const pc8801::Config& config);
  void LoadNewConfig();
  // TODO: temporary for migration
  pc8801::Config& config() { return config_; }

 private:
  ConfigService() = default;
  static void Init();

  // singleton instance
  static ConfigService instance_;
  static std::once_flag once_;
  pc8801::Config config_{};
  pc8801::Config new_config_{};
};
}  // namespace services
