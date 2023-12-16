// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999, 2000.
// ---------------------------------------------------------------------------
//  DirectSound based driver - another version
// ---------------------------------------------------------------------------
//  $Id: soundds2.h,v 1.2 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <windows.h>

#include <mmsystem.h>
#include <dsound.h>

#include "common/scoped_comptr.h"
#include "common/threadable.h"
#include "win32/sound/sounddrv.h"

// ---------------------------------------------------------------------------

namespace win32sound {

class DriverDS2 : public Driver, public Threadable<DriverDS2> {
 private:
  enum {
    nblocks = 4,  // 2 以上
  };

 public:
  DriverDS2();
  ~DriverDS2() override;

  bool Init(SoundSource16* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms) override;
  bool CleanUp() override;

  void ThreadInit();
  bool ThreadLoop();

 private:
  void Send();

  scoped_comptr<IDirectSound8> lpds_;
  scoped_comptr<IDirectSoundBuffer> lpdsb_primary_;
  scoped_comptr<IDirectSoundBuffer> lpdsb_;
  scoped_comptr<IDirectSoundNotify> lpnotify_;

  uint32_t buffer_length_ms_ = 0;
  HANDLE hevent_ = nullptr;
  uint32_t nextwrite_ = 0;
};

}  // namespace win32sound
