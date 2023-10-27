// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: module.h,v 1.2 1999/10/10 15:59:54 cisc Exp $

#pragma once

#include "if/ifcommon.h"

#include <string_view>

namespace pc8801 {

class ExtendModule {
 public:
  ExtendModule();
  ~ExtendModule();

  static ExtendModule* Create(const std::string_view dllname, ISystem* pc);

  bool Connect(const std::string_view dllname, ISystem* pc);
  bool Disconnect();

  IDevice::ID GetID();
  void* QueryIF(REFIID iid);

 private:
  typedef IModule*(__cdecl* F_CONNECT2)(ISystem*);

  HMODULE hdll_ = nullptr;
  IModule* mod_ = nullptr;
};

inline void* ExtendModule::QueryIF(REFIID iid) {
  return mod_ ? mod_->QueryIF(iid) : nullptr;
}

}  // namespace pc8801
