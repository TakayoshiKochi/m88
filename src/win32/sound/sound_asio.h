#pragma once

#include <windows.h>

#include "common/threadable.h"
#include "win32/sound/sounddrv.h"

#include <mutex>

struct ASIOTime;

namespace WinSoundDriver {

class DriverASIO : public Driver, public Threadable<DriverASIO> {
 private:
  enum {
    nblocks = 4,  // 2 以上
  };

 public:
  DriverASIO();
  ~DriverASIO() override;

  bool Init(SoundSource* source,
            HWND hwnd,
            uint32_t rate,
            uint32_t ch,
            uint32_t buflen_ms) override;
  bool CleanUp() override;

  void ThreadInit();
  bool ThreadLoop();

  // callback interface
  void BufferSwitch(int index);
  ASIOTime* BufferSwitchTimeInfo(ASIOTime* time, long index);
  void SampleRateChanged(double rate);
  long AsioMessage(long selector, long value, void* message, double* opt);

 private:
  void Send();
};

}  // namespace WinSoundDriver
