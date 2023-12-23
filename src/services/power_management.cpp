// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "services/power_management.h"

namespace services {

// static
PowerManagement PowerManagement::instance_;
// static
std::once_flag PowerManagement::once_;

// static
void PowerManagement::Init() {
  instance_.InitInstance();
}

void PowerManagement::InitInstance() {
  REASON_CONTEXT ctx{};
  ctx.Version = POWER_REQUEST_CONTEXT_VERSION;
  ctx.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
  ctx.Reason.SimpleReasonString = const_cast<LPWSTR>(L"M88 wakelock for fulllscreen");

  hpower_.reset(PowerCreateRequest(&ctx));
  if (hpower_.get() == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    hpower_.reset();
  }
}

void PowerManagement::PreventSleep() const {
  if (hpower_) {
    PowerSetRequest(hpower_.get(), PowerRequestDisplayRequired);
    PowerSetRequest(hpower_.get(), PowerRequestSystemRequired);
  } else {
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
  }
}

void PowerManagement::AllowSleep() const {
  if (hpower_) {
    PowerClearRequest(hpower_.get(), PowerRequestDisplayRequired);
    PowerClearRequest(hpower_.get(), PowerRequestSystemRequired);
  } else {
    SetThreadExecutionState(ES_CONTINUOUS);
  }
}

} // namespace services
