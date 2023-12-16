// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

#include <windows.h>

#include "common/threadable.h"
#include "win32/sound/sounddrv.h"
#include "win32/sound/winsound.h"

#include <string>
#include <mutex>
#include <vector>

struct ASIOTime;

namespace win32sound {

class DriverASIO : public Driver {
 private:
  enum {
    nblocks = 4,  // 2 以上
  };

 public:
  DriverASIO();
  ~DriverASIO() override;

  bool Init(SoundSource16* source,
            HWND hwnd,
            uint32_t rate,
            uint32_t ch,
            uint32_t buflen_ms) override;
  bool CleanUp() override;

  void SetDriverName(const std::string& name) override { preferred_driver_ = name; }
  uint32_t GetSampleRate() override { return sample_rate_; }
  std::string GetDriverName() override { return current_driver_; }

  // callback interface
  void BufferSwitch(int index);
  ASIOTime* BufferSwitchTimeInfo(ASIOTime* time, long index);
  void SampleRateChanged(double rate);
  long AsioMessage(long selector, long value, void* message, double* opt);

 private:
  void FindAvailableDriver();
  void Start();
  void Stop();

  std::string preferred_driver_;
  std::string current_driver_;
  std::vector<std::string> available_drivers_;
  uint32_t sample_rate_ = 48000;
};

}  // namespace win32sound
