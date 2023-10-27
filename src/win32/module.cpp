// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: module.cpp,v 1.1 1999/10/10 01:47:21 cisc Exp $

#include "win32/module.h"

using namespace pc8801;

ExtendModule::ExtendModule() = default;

ExtendModule::~ExtendModule() {
  Disconnect();
}

bool ExtendModule::Connect(const std::string_view dllname, ISystem* pc) {
  Disconnect();

  hdll_ = LoadLibrary(dllname.data());
  if (!hdll_)
    return false;

  auto conn = (F_CONNECT2)GetProcAddress(hdll_, "M88CreateModule");
  if (!conn)
    return false;

  mod_ = (*conn)(pc);
  return mod_ != nullptr;
}

bool ExtendModule::Disconnect() {
  if (mod_) {
    mod_->Release();
    mod_ = nullptr;
  }
  if (hdll_) {
    FreeLibrary(hdll_);
    hdll_ = nullptr;
  }
  return true;
}

ExtendModule* ExtendModule::Create(const std::string_view dllname, ISystem* pc) {
  auto* em = new ExtendModule;
  if (em) {
    if (em->Connect(dllname, pc))
      return em;
    delete em;
  }
  return nullptr;
}
