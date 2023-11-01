// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  CD-ROM インターフェースの実装
// ----------------------------------------------------------------------------
//  $Id: cdif.h,v 1.2 1999/10/10 01:39:00 cisc Exp $

#pragma once

#include <windows.h>

#include "cdctrl.h"
#include "cdrom.h"
#include "common/device.h"
#include "if/ifpc88.h"

namespace pc8801 {

class CDIF : public Device {
 public:
  enum IDIn {
    in90 = 0,
    in91,
    in92,
    in93,
    in96,
    in98,
    in99,
    in9b,
    in9d,
  };
  enum IDOut {
    reset = 0,
    out90,
    out91,
    out94,
    out97,
    out98,
    out99,
    out9f,
  };

 public:
  explicit CDIF(const ID& id);
  ~CDIF() override;
  bool Init(IDMAAccess* dmac);

  bool Enable(bool f);

  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  void IOCALL SystemReset(uint32_t, uint32_t d);
  void IOCALL Out90(uint32_t, uint32_t d);
  void IOCALL Out91(uint32_t, uint32_t d);
  void IOCALL Out94(uint32_t, uint32_t d);
  void IOCALL Out97(uint32_t, uint32_t d);
  void IOCALL Out98(uint32_t, uint32_t d);
  void IOCALL Out99(uint32_t, uint32_t d);
  void IOCALL Out9f(uint32_t, uint32_t d);
  uint32_t IOCALL In90(uint32_t);
  uint32_t IOCALL In91(uint32_t);
  uint32_t IOCALL In92(uint32_t);
  uint32_t IOCALL In93(uint32_t);
  uint32_t IOCALL In96(uint32_t);
  uint32_t IOCALL In98(uint32_t);
  uint32_t IOCALL In99(uint32_t);
  uint32_t IOCALL In9b(uint32_t);
  uint32_t IOCALL In9d(uint32_t);

  void Reset();

 private:
  enum Phase {
    kIdlePhase,
    kCmd1Phase,
    kCmd2Phase,
    kParamPhase,
    kExecPhase,
    kWaitPhase,
    kResultPhase,
    kStatusPhase,
    kSendPhase,
    kEndPhase,
    kRecvPhase
  };
  typedef void (CDIF::*CommandFunc)();

 private:
  void DataIn();
  void DataOut();
  void ProcessCommand();
  void ResultPhase(int r, int s);
  void SendPhase(int b, int r, int s);
  void RecvPhase(int b);
  void SendCommand(uint32_t a, WPARAM b = 0, LPARAM c = 0);
  void Done(int ret);

  void CheckDriveStatus();
  void ReadTOC();
  void TrackSearch();
  void ReadSubcodeQ();
  void PlayStart();
  void PlayStop();
  void SetReadMode();
  void ReadSector();

  uint32_t GetPlayAddress();

  enum {
    ssrev = 1,
  };
  struct Snapshot {
    uint8_t rev;
    uint8_t phase;
    uint8_t status;
    uint8_t data;
    uint8_t playmode;
    uint8_t retrycount;
    uint8_t stillmode;
    uint8_t rslt;
    uint8_t stat;

    uint32_t sector;
    uint16_t ptr;
    uint16_t length;
    uint32_t addrs;

    uint8_t buf[16 + 16 + 2340];
  };

  IDMAAccess* dmac_ = nullptr;

  int phase_ = 0;

  uint8_t* ptr_ = nullptr;  // 転送モード
  int length_ = 0;

  uint32_t addrs_;  // 再生開始アドレス

  uint32_t status_ = 0;     // in 90
  uint32_t data_ = 0;       // port 91
  uint32_t play_mode_ = 0;  // port 98
  uint32_t retry_count_ = 0;
  uint32_t read_mode_ = 0;
  uint32_t sector_ = 0;

  uint8_t still_mode_ = 0;

  uint8_t clk_ = 0;
  uint8_t rslt_ = 0;
  uint8_t stat_ = 0;
  bool enable_ = false;
  bool active_ = false;

  uint8_t cmdbuf_[16]{};  // バッファは連続して配置されること
  uint8_t databuf_[16]{};
  uint8_t tmpbuf_[2340]{};

  CDROM cdrom_;
  CDControl cd_;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
