// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id

#pragma once

#include <windows.h>

#include <ddraw.h>

class ExtendedAPIAccessBase {
 public:
  ExtendedAPIAccessBase(const char* dllname, const char* apiname, void* dummy);
  [[nodiscard]] bool IsValid() const { return valid_; }

 protected:
  void* Method() { return method_; }

 private:
  void* method_ = nullptr;
  bool valid_ = false;
};

class ExternalDLLAccessBase {
 public:
  ExternalDLLAccessBase(const char* dllname, const char* apiname, void* dummy)
      : method_(nullptr), dllname_(dllname), apiname_(apiname), dummy_(dummy) {}
  ~ExternalDLLAccessBase();

  bool IsValid() {
    Method();
    return hmod_ != nullptr;
  }

 protected:
  void* Method() {
    if (!method_)
      Load();
    return method_;
  }

 private:
  void Load();
  void* method_;
  const char* dllname_;
  const char* apiname_;
  union {
    void* dummy_;
    HMODULE hmod_;
  };
};

template <class T, class Base>
class ExtendedAPIAccess : public Base {
 public:
  ExtendedAPIAccess(const char* dllname, const char* apiname, T dummy)
      : Base(dllname, apiname, (void*)dummy) {}

  operator T() { return reinterpret_cast<T>(Base::Method()); }
};

#ifndef DECLARE_EXAPI
#define DECLARE_EXAPI(name, type, arg, dll, api, def) \
  extern ExtendedAPIAccess<type(WINAPI*) arg, ExtendedAPIAccessBase> name;
#endif

#ifndef DECLARE_EXDLL
#define DECLARE_EXDLL(name, type, arg, dll, api, def) \
  extern ExtendedAPIAccess<type(WINAPI*) arg, ExternalDLLAccessBase> name;
#endif

// ---------------------------------------------------------------------------
//  標準では提供されないかもしれない API を列挙
//  EXAPI - GetModuleHandle で接続
//  EXDLL - 起動時 に LoadLibrary で接続
//
DECLARE_EXAPI(EnableIME, BOOL, (HWND, BOOL), "user32.dll", "WINNLSEnableIME", FALSE)
DECLARE_EXAPI(MonitorFromWin, HMONITOR, (HWND, DWORD), "user32.dll", "MonitorFromWindow", 0)
DECLARE_EXDLL(DDEnumerateEx,
              HRESULT,
              (LPDDENUMCALLBACKEX, LPVOID, DWORD),
              "ddraw.dll",
              "DirectDrawEnumerateExA",
              S_OK)
