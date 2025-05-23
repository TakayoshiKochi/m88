// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  $Id: wincore.h,v 1.34 2003/05/15 13:15:36 cisc Exp $

#pragma once

// ---------------------------------------------------------------------------

#include <vector>

#include <stdint.h>

#include "common/emulation_loop.h"
#include "pc88/config.h"
#include "pc88/pc88.h"
#include "win32/winjoy.h"
#include "win32/sound/winsound.h"

class ExtendModule;
class ExternalDevice;
class WinKeyIF;
class WinUI;

// ---------------------------------------------------------------------------

class WinCore : public ISystem, public ILockCore {
 public:
  WinCore() = default;
  ~WinCore();
  bool Init(HWND hwnd,
            WinKeyIF* keyb,
            IConfigPropBase* cpb,
            Draw* draw,
            services::DiskManager* diskmgr,
            services::TapeManager* tapemgr);
  bool CleanUp();

  void Reset();
  void ApplyConfig(const pc8801::Config* config);
  void ApplyConfig2(const pc8801::Config* config);

  bool SaveSnapshot(const std::string_view filename);
  bool LoadSnapshot(const std::string_view filename, const std::string_view diskname);

  PC88* GetPC88() { return &pc88_; }
  WinSound* GetSound() { return &sound_; }

  int64_t GetExecClocks() { return seq_.GetExecClocks(); }
  void Wait(bool dowait) { dowait ? seq_.Deactivate() : seq_.Activate(); }
  void* IFCALL QueryIF(REFIID iid) override;
  void IFCALL Lock() override { seq_.Lock(); }
  void IFCALL Unlock() override { seq_.Unlock(); }

 private:
  //  Snapshot ヘッダー
  enum {
    ssmajor = 1,
    ssminor = 1,
  };

  struct SnapshotHeader {
    char id[16];
    uint8_t major, minor;

    int8_t disk[2];
    int datasize;
    pc8801::BasicMode basic_mode;
    int16_t legacy_clock;
    uint16_t erambanks;
    uint16_t cpumode;
    uint16_t mainsubratio;
    pc8801::Config::Flags flags;
    pc8801::Config::Flag2 flag2;
  };

  // Lock between UI thread and emulation thread.
  class LockObj {
   public:
    LockObj(WinCore* c) : core_(c) { core_->Lock(); }
    ~LockObj() { core_->Unlock(); }

   private:
    WinCore* core_;
  };

 private:
  bool ConnectDevices(WinKeyIF* keyb);
  bool ConnectExternalDevices();

  HWND hwnd_ = nullptr;
  PC88 pc88_;
  IConfigPropBase* cfg_prop_ = nullptr;

  EmulationLoop seq_;
  WinPadIF pad_if_;

  using ExtendModules = std::vector<ExtendModule*>;
  ExtendModules ext_modules_;

  WinSound sound_;
  const pc8801::Config* config_;

  using ExternalDevices = std::vector<ExternalDevice*>;
  ExternalDevices ext_devices_;
};
