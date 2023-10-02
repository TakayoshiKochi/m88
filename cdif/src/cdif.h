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

namespace PC8801 {

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
    idlephase,
    cmd1phase,
    cmd2phase,
    paramphase,
    execphase,
    waitphase,
    resultphase,
    statusphase,
    sendphase,
    endphase,
    recvphase
  };
  typedef void (CDIF::*CommandFunc)();

 private:
  void DataIn();
  void DataOut();
  void ProcessCommand();
  void ResultPhase(int r, int s);
  void SendPhase(int b, int r, int s);
  void RecvPhase(int b);
  void SendCommand(uint32_t c, uint32_t a1 = 0, uint32_t a2 = 0);
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

  IDMAAccess* dmac_;

  int phase;

  uint8_t* ptr;  // 転送モード
  int length;

  uint32_t addrs;  // 再生開始アドレス

  uint32_t status;    // in 90
  uint32_t data;      // port 91
  uint32_t playmode;  // port 98
  uint32_t retrycount;
  uint32_t readmode;
  uint32_t sector;

  uint8_t stillmode;

  uint8_t clk;
  uint8_t rslt;
  uint8_t stat;
  bool enable;
  bool active;

  uint8_t cmdbuf[16];  // バッファは連続して配置されること
  uint8_t datbuf[16];
  uint8_t tmpbuf[2340];
  CDROM cdrom;
  CDControl cd;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

};  // namespace PC8801
