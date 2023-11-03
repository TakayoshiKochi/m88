// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: wincore.cpp,v 1.42 2003/05/15 13:15:35 cisc Exp $

#include "win32/wincore.h"

#include "common/device.h"
#include "if/ifguid.h"
#include "if/ifpc88.h"
#include "pc88/beep.h"
#include "pc88/config.h"
#include "pc88/joypad.h"
#include "pc88/opnif.h"
#include "pc88/pd8257.h"
#include "services/diskmgr.h"
#include "win32/extdev.h"
#include "win32/file.h"
#include "win32/messages.h"
#include "win32/module.h"
#include "win32/status_win.h"
#include "win32/ui.h"
#include "win32/version.h"
#include "win32/winkeyif.h"
#include "zlib/zlib.h"

// #define LOGNAME "wincore"
#include "common/diag.h"

//                   0123456789abcdef
#define SNAPSHOT_ID "M88 SnapshotData"

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinCore::~WinCore() {
  pc88_.DeInit();
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WinCore::Init(HWND hwnd,
                   WinKeyIF* keyb,
                   IConfigPropBase* cp,
                   Draw* draw,
                   services::DiskManager* disk,
                   services::TapeManager* tape) {
  hwnd_ = hwnd;
  cfg_prop_ = cp;

  if (!pc88_.Init(draw, disk, tape))
    return false;

  if (!sound_.Init(&pc88_, hwnd, 0, 0))
    return false;

  pad_if_.Init();

  if (!ConnectDevices(keyb))
    return false;

  if (!ConnectExternalDevices())
    return false;

  // seq.SetClock(40);   // 4MHz
  seq_.SetCPUClock(3993600);
  seq_.SetSpeed(100);  // 100%

  return seq_.Init(&pc88_);
}

// ---------------------------------------------------------------------------
//  後始末
//
bool WinCore::CleanUp() {
  seq_.CleanUp();

  for (ExtendModules::iterator i = ext_modules_.begin(); i != ext_modules_.end(); ++i)
    delete *i;
  ext_modules_.clear();

  for (ExternalDevices::iterator j = ext_devices_.begin(); j != ext_devices_.end(); ++j)
    delete *j;
  ext_devices_.clear();

  return true;
}

// ---------------------------------------------------------------------------
//  リセット
//
void WinCore::Reset() {
  LockObj lock(this);
  pc88_.Reset();
}

// ---------------------------------------------------------------------------
//  設定を反映する
//
void WinCore::ApplyConfig(const pc8801::Config* config) {
  config_ = config;
  // XXX
  ApplyConfig2(config_);
}

void WinCore::ApplyConfig2(const pc8801::Config* config) {
  int c = config->legacy_clock;
  uint64_t cpu_clock = c * 100000;
  if (c == 40) {
    cpu_clock = 3993600;
  } else if (c == 80) {
    cpu_clock = 7987200;
  }
  if (config->flags & pc8801::Config::kFullSpeed)
    c = 0;
  if (config->flags & pc8801::Config::kCPUBurst)
    c = -c;
  seq_.SetLegacyClock(c);
  seq_.SetCPUClock(cpu_clock);
  seq_.SetSpeed(config->speed / 10);

  if (pc88_.GetJoyPad())
    pc88_.GetJoyPad()->Connect(&pad_if_);

  pc88_.ApplyConfig(config);
  sound_.ApplyConfig(config);
}

// ---------------------------------------------------------------------------
//  Windows 用のデバイスを接続
//
bool WinCore::ConnectDevices(WinKeyIF* keyb) {
  static const IOBus::Connector c_keyb[] = {{PC88::kPReset, IOBus::portout, WinKeyIF::reset},
                                            {PC88::kVrtc, IOBus::portout, WinKeyIF::vsync},
                                            {0x00, IOBus::portin, WinKeyIF::in},
                                            {0x01, IOBus::portin, WinKeyIF::in},
                                            {0x02, IOBus::portin, WinKeyIF::in},
                                            {0x03, IOBus::portin, WinKeyIF::in},
                                            {0x04, IOBus::portin, WinKeyIF::in},
                                            {0x05, IOBus::portin, WinKeyIF::in},
                                            {0x06, IOBus::portin, WinKeyIF::in},
                                            {0x07, IOBus::portin, WinKeyIF::in},
                                            {0x08, IOBus::portin, WinKeyIF::in},
                                            {0x09, IOBus::portin, WinKeyIF::in},
                                            {0x0a, IOBus::portin, WinKeyIF::in},
                                            {0x0b, IOBus::portin, WinKeyIF::in},
                                            {0x0c, IOBus::portin, WinKeyIF::in},
                                            {0x0d, IOBus::portin, WinKeyIF::in},
                                            {0x0e, IOBus::portin, WinKeyIF::in},
                                            {0x0f, IOBus::portin, WinKeyIF::in},
                                            {0, 0, 0}};
  if (!pc88_.GetBus1()->Connect(keyb, c_keyb))
    return false;

  if (FAILED(pc88_.GetOPN1()->Connect(&sound_)))
    return false;
  if (FAILED(pc88_.GetOPN2()->Connect(&sound_)))
    return false;
  if (FAILED(pc88_.GetBEEP()->Connect(&sound_)))
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  スナップショット保存
//
bool WinCore::SaveSnapshot(const std::string_view filename) {
  LockObj lock(this);

  uint32_t size = pc88_.GetDeviceList()->GetStatusSize();
  std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(size * 129 / 64 + 20);
  if (!buf)
    return false;
  memset(buf.get(), 0, size);

  if (pc88_.GetDeviceList()->SaveStatus(buf.get())) {
    uint32_t esize = size * 129 / 64 + 20 - 4;
    if (Z_OK != compress(buf.get() + size + 4, (uLongf*)&esize, buf.get(), size)) {
      return false;
    }
    *(int32_t*)(buf.get() + size) = -(long)esize;
    esize += 4;

    SnapshotHeader ssh;
    memcpy(ssh.id, SNAPSHOT_ID, 16);

    ssh.major = ssmajor;
    ssh.minor = ssminor;
    ssh.datasize = size;
    ssh.basicmode = config_->basicmode;
    ssh.legacy_clock = int16_t(config_->legacy_clock);
    ssh.erambanks = uint16_t(config_->erambanks);
    ssh.cpumode = int16_t(config_->cpumode);
    ssh.mainsubratio = int16_t(config_->mainsubratio);
    ssh.flags = config_->flags | (esize < size ? 0x80000000 : 0);
    ssh.flag2 = config_->flag2;
    for (uint32_t i = 0; i < 2; i++)
      ssh.disk[i] = (int8_t)pc88_.GetDiskManager()->GetCurrentDisk(i);

    FileIOWin file;
    if (file.Open(filename, FileIO::create)) {
      file.Write(&ssh, sizeof(ssh));
      if (esize < size)
        file.Write(buf.get() + size, esize);
      else
        file.Write(buf.get(), size);
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
//  スナップショット復元
//
bool WinCore::LoadSnapshot(const std::string_view filename, const std::string_view diskname) {
  LockObj lock(this);

  FileIOWin file;
  if (!file.Open(filename, FileIO::readonly))
    return false;

  SnapshotHeader ssh;
  if (file.Read(&ssh, sizeof(ssh)) != sizeof(ssh))
    return false;
  if (memcmp(ssh.id, SNAPSHOT_ID, 16))
    return false;
  if (ssh.major != ssmajor || ssh.minor > ssminor)
    return false;

  // applyconfig
  const uint32_t fl1a =
      pc8801::Config::kSubCPUControl | pc8801::Config::kFullSpeed | pc8801::Config::kEnableOPNA |
      pc8801::Config::kEnablePCG | pc8801::Config::kFv15k | pc8801::Config::kCPUBurst |
      pc8801::Config::kCPUClockMode | pc8801::Config::kDigitalPalette | pc8801::Config::kOPNonA8 |
      pc8801::Config::kOPNAonA8 | pc8801::Config::kEnableWait;
  const uint32_t fl2a = pc8801::Config::kDisableOPN44;

  pc8801::Config newconfig = *config_;

  newconfig.flags = (config_->flags & ~fl1a) | (ssh.flags & fl1a);
  newconfig.flag2 = (config_->flag2 & ~fl2a) | (ssh.flag2 & fl2a);
  newconfig.basicmode = ssh.basicmode;
  newconfig.legacy_clock = ssh.legacy_clock;
  newconfig.erambanks = ssh.erambanks;
  newconfig.cpumode = ssh.cpumode;
  newconfig.mainsubratio = ssh.mainsubratio;

  // TODO: This is a temporary hack.
  // PostMessage (below in this function) will take effect asynchronously - can't be used here
  // (needs to be synchronously applied before Reset).
  ApplyConfig2(&newconfig);

  // Reset
  pc88_.Reset();

  // 読み込み

  std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(ssh.datasize);
  bool r = false;

  if (buf) {
    bool read = false;
    if (ssh.flags & 0x80000000) {
      int32_t csize;

      file.Read(&csize, 4);
      if (csize < 0) {
        csize = -csize;
        std::unique_ptr<uint8_t[]> cbuf = std::make_unique<uint8_t[]>(csize);

        if (cbuf) {
          uint32_t bufsize = ssh.datasize;
          file.Read(cbuf.get(), csize);
          read = uncompress(buf.get(), (uLongf*)&bufsize, cbuf.get(), csize) == Z_OK;
        }
      }
    } else {
      read = file.Read(buf.get(), ssh.datasize) == ssh.datasize;
    }

    if (read) {
      r = pc88_.GetDeviceList()->LoadStatus(buf.get());
      if (r && !diskname.empty()) {
        for (uint32_t i = 0; i < 2; i++) {
          pc88_.GetDiskManager()->Unmount(i);
          pc88_.GetDiskManager()->Mount(i, diskname, false, ssh.disk[i], false);
        }
      }
      if (!r) {
        statusdisplay.Show(70, 3000, "バージョンが異なります");
        pc88_.Reset();
      }
    }
  }
  services::ConfigService::GetInstance()->UpdateConfig(newconfig);
  PostMessage(hwnd_, WM_M88_APPLYCONFIG, 0, 0);
  return r;
}

// ---------------------------------------------------------------------------
//  外部もじゅーるのためにインターフェースを提供する
//
void* WinCore::QueryIF(REFIID id) {
  if (id == M88IID_IOBus1)
    return static_cast<IIOBus*>(pc88_.GetBus1());
  if (id == M88IID_IOBus2)
    return static_cast<IIOBus*>(pc88_.GetBus2());
  if (id == M88IID_IOAccess1)
    return static_cast<IIOAccess*>(pc88_.GetBus1());
  if (id == M88IID_IOAccess2)
    return static_cast<IIOAccess*>(pc88_.GetBus2());
  if (id == M88IID_MemoryManager1)
    return static_cast<IMemoryManager*>(pc88_.GetMM1());
  if (id == M88IID_MemoryManager2)
    return static_cast<IMemoryManager*>(pc88_.GetMM2());
  if (id == M88IID_MemoryAccess1)
    return static_cast<IMemoryAccess*>(pc88_.GetMM1());
  if (id == M88IID_MemoryAccess2)
    return static_cast<IMemoryAccess*>(pc88_.GetMM2());
  if (id == M88IID_SoundControl)
    return static_cast<ISoundControl*>(&sound_);
  if (id == M88IID_Scheduler)
    return static_cast<IScheduler*>(pc88_.GetScheduler());
  if (id == M88IID_Time)
    return static_cast<ITime*>(pc88_.GetScheduler());
  if (id == M88IID_CPUTime)
    return static_cast<ICPUTime*>(&pc88_);
  if (id == M88IID_DMA)
    return static_cast<IDMAAccess*>(pc88_.GetDMAC());
  if (id == M88IID_ConfigPropBase)
    return cfg_prop_;
  if (id == M88IID_LockCore)
    return static_cast<ILockCore*>(this);
  if (id == M88IID_GetMemoryBank)
    return static_cast<IGetMemoryBank*>(pc88_.GetMem1());

  return nullptr;
}

bool WinCore::ConnectExternalDevices() {
  FileFinder ff;
  extern char m88dir[MAX_PATH];
  char buf[MAX_PATH];
  strncpy_s(buf, MAX_PATH, m88dir, _TRUNCATE);
  strncat_s(buf, MAX_PATH, "*.m88", _TRUNCATE);

  if (ff.FindFile(buf)) {
    while (ff.FindNext()) {
      const char* modname = ff.GetFileName();
      ExtendModule* em = ExtendModule::Create(modname, this);
      if (em) {
        ext_modules_.push_back(em);
      } else {
        auto* extdevice = new ExternalDevice();
        if (extdevice) {
          if (extdevice->Init(modname, pc88_.GetScheduler(), pc88_.GetBus1(), pc88_.GetDMAC(),
                              &sound_, pc88_.GetMM1())) {
            pc88_.GetDeviceList()->Add(extdevice);
            ext_devices_.push_back(extdevice);
          } else {
            delete extdevice;
          }
        }
      }
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
//  PAD を接続
//
/*
bool WinCore::EnablePad(bool enable)
{
    if (padenable == enable)
        return true;
    padenable = enable;

    LockObj lock(this);
    if (enable)
    {
        static const IOBus::Connector c_pad[] =
        {
            { PC88::popnio  , IOBus::portin, WinJoyPad::getdir },
            { PC88::popnio+1, IOBus::portin, WinJoyPad::getbutton },
            { PC88::vrtc,     IOBus::portout, WinJoyPad::vsync },
            { 0, 0, 0 }
        };
        if (!bus1.Connect(&pad, c_pad)) return false;
        pad.Init();
    }
    else
    {
        bus1.Disconnect(&pad);
    }
    return true;
}
*/
// ---------------------------------------------------------------------------
//  マウスを接続
//
/*
bool WinCore::EnableMouse(bool enable)
{
    if (mouseenable == enable)
        return true;
    mouseenable = enable;

    LockObj lock(this);
    if (enable)
    {
        static const IOBus::Connector c_mouse[] =
        {
            { PC88::popnio  , IOBus::portin, WinMouse::getmove },
            { PC88::popnio+1, IOBus::portin, WinMouse::getbutton },
            { 0x40,           IOBus::portout, WinMouse::strobe },
            { PC88::vrtc,     IOBus::portout, WinMouse::vsync },
            { 0, 0, 0 }
        };
        if (!bus1.Connect(&mouse, c_mouse)) return false;
        ActivateMouse(true);
    }
    else
    {
        bus1.Disconnect(&mouse);
        ActivateMouse(false);
    }
    return true;
}
*/
