//  $Id: winexapi.cpp,v 1.1 2002/04/07 05:40:11 cisc Exp $

#define DECLARE_EXAPI(name, type, arg, dll, api, def) \
  static type WINAPI ef_##name arg {                  \
    return def;                                       \
  }                                                   \
  ExtendedAPIAccess<type(WINAPI*) arg, ExtendedAPIAccessBase> name(dll, api, ef_##name);

#define DECLARE_EXDLL(name, type, arg, dll, api, def) \
  static type WINAPI ef_##name arg {                  \
    return def;                                       \
  }                                                   \
  ExtendedAPIAccess<type(WINAPI*) arg, ExternalDLLAccessBase> name(dll, api, ef_##name);

#include "win32/winexapi.h"

ExtendedAPIAccessBase::ExtendedAPIAccessBase(const char* dllname,
                                             const char* apiname,
                                             void* dummy) {
  method_ = dummy;
  valid_ = false;
  HMODULE hmod = ::GetModuleHandle(dllname);
  if (hmod) {
    method_ = (void*)::GetProcAddress(hmod, apiname);
    if (!method_)
      method_ = dummy;
    valid_ = true;
  }
}

void ExternalDLLAccessBase::Load() {
  method_ = dummy_;
  hmod_ = ::LoadLibrary(dllname_);
  if (hmod_) {
    auto* addr = (void*)::GetProcAddress(hmod_, apiname_);
    if (addr) {
      method_ = addr;
    } else {
      ::FreeLibrary(hmod_);
      hmod_ = nullptr;
    }
  }
}

ExternalDLLAccessBase::~ExternalDLLAccessBase() {
  if (hmod_)
    ::FreeLibrary(hmod_);
}
