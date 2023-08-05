// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  uPD765A
// ---------------------------------------------------------------------------
//  $Id: fdc.h,v 1.12 2001/02/21 11:57:57 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/scheduler.h"
#include "pc88/fdu.h"
#include "pc88/floppy.h"

class DiskManager;
class IOBus;

namespace PC8801 {
class Config;

// ---------------------------------------------------------------------------
//  FDC (765)
//
class FDC : public Device {
 public:
  enum {
    num_drives = 2,
  };
  enum ConnID { reset = 0, setdata, drivecontrol, motorcontrol, getstatus = 0, getdata, tcin };

  enum Result {
    ST0_NR = 0x000008,
    ST0_EC = 0x000010,
    ST0_SE = 0x000020,
    ST0_AT = 0x000040,
    ST0_IC = 0x000080,
    ST0_AI = 0x0000c0,

    ST1_MA = 0x000100,
    ST1_NW = 0x000200,
    ST1_ND = 0x000400,
    ST1_OR = 0x001000,
    ST1_DE = 0x002000,
    ST1_EN = 0x008000,

    ST2_MD = 0x010000,
    ST2_BC = 0x020000,
    ST2_SN = 0x040000,
    ST2_SH = 0x080000,
    ST2_NC = 0x100000,
    ST2_DD = 0x200000,
    ST2_CM = 0x400000,

    ST3_HD = 0x04,
    ST3_TS = 0x08,
    ST3_T0 = 0x10,
    ST3_RY = 0x20,
    ST3_WP = 0x40,
    ST3_FT = 0x80,
  };
  typedef FloppyDisk::IDR IDR;
  typedef FDU::WIDDESC WIDDESC;

 protected:
  enum Stat {
    S_D0B = 0x01,
    S_D1B = 0x02,
    S_D2B = 0x04,
    S_D3B = 0x08,
    S_CB = 0x10,
    S_NDM = 0x20,
    S_DIO = 0x40,
    S_RQM = 0x80,
  };

 public:
  FDC(const ID& id);
  ~FDC();

  bool Init(DiskManager* dm, Scheduler* scheduler, IOBus* bus, int intport, int statport = -1);
  void ApplyConfig(const Config* cfg);

  bool IsBusy() { return phase != idlephase; }

  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL DriveControl(uint32_t, uint32_t data);    // 2HD/2DD 切り替えとか
  void IOCALL MotorControl(uint32_t, uint32_t data) {}  // モーター制御
  void IOCALL SetData(uint32_t, uint32_t data);         // データセット
  uint32_t IOCALL TC(uint32_t);                         // TC
  uint32_t IOCALL Status(uint32_t);                     // ステータス入力
  uint32_t IOCALL GetData(uint32_t);                    // データ取得

  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

  const Descriptor* IFCALL GetDesc() const { return &descriptor; }

 private:
  enum Phase {
    idlephase,
    commandphase,
    executephase,
    execreadphase,
    execwritephase,
    resultphase,
    tcphase,
    timerphase,
    execscanphase,
  };

  struct Drive {
    uint32_t cyrinder;
    uint32_t result;
    uint8_t hd;
    uint8_t dd;
  };

  enum {
    ssrev = 1,
  };
  struct Snapshot {
    uint8_t rev;
    uint8_t hdu;  // HD US1 US0
    uint8_t hdue;
    uint8_t dtl;
    uint8_t eot;
    uint8_t seekstate;
    uint8_t result;
    uint8_t status;      // ステータスレジスタ
    uint8_t command;     // 現在処理中のコマンド
    uint8_t data;        // データレジスタ
    bool int_requested;  // SENCEINTSTATUS の呼び出しを要求した
    bool accepttc;

    uint32_t bufptr;
    uint32_t count;  // Exec*Phase での転送残りバイト
    Phase phase, prevphase;
    Phase t_phase;

    IDR idr;

    uint32_t readdiagptr;
    uint32_t readdiaglim;
    uint32_t xbyte;
    uint32_t readdiagcount;

    WIDDESC wid;
    Drive dr[num_drives];
    uint8_t buf[0x4000];
  };

  typedef void (FDC::*CommandFunc)();

  void Seek(uint32_t dr, uint32_t cy);
  void IOCALL SeekEvent(uint32_t dr);
  void ReadID();
  void ReadData(bool deleted, bool scan);
  void ReadDiagnostic();
  void WriteData(bool deleted);
  void WriteID();

  void SetTimer(Phase phase, int ticks);
  void DelTimer();
  void IOCALL PhaseTimer(uint32_t p);
  void Intr(bool i);

  bool IDIncrement();
  void GetSectorParameters();
  uint32_t CheckCondition(bool write);

  void ShiftToIdlePhase();
  void ShiftToCommandPhase(int);
  void ShiftToExecutePhase();
  void ShiftToExecReadPhase(int, uint8_t* data = 0);
  void ShiftToExecWritePhase(int);
  void ShiftToExecScanPhase(int);
  void ShiftToResultPhase(int);
  void ShiftToResultPhase7();

  uint32_t GetDeviceStatus(uint32_t dr);

  DiskManager* diskmgr;
  IOBus* bus;
  int pintr;
  Scheduler* scheduler;

  Scheduler::Event* timerhandle;
  uint32_t seektime;

  uint32_t status;  // ステータスレジスタ
  uint8_t* buffer;
  uint8_t* bufptr;
  int count;         // Exec*Phase での転送残りバイト
  uint32_t command;  // 現在処理中のコマンド
  uint32_t data;     // データレジスタ
  Phase phase, prevphase;
  Phase t_phase;
  bool int_requested;  // SENCEINTSTATUS の呼び出しを要求した
  bool accepttc;
  bool showstatus;

  bool diskwait;

  IDR idr;
  uint32_t hdu;  // HD US1 US0
  uint32_t hdue;
  uint32_t dtl;
  uint32_t eot;
  uint32_t seekstate;
  uint32_t result;

  uint8_t* readdiagptr;
  uint8_t* readdiaglim;
  uint32_t xbyte;
  uint32_t readdiagcount;

  uint32_t litdrive;
  uint32_t fdstat;
  uint32_t pfdstat;

  WIDDESC wid;
  Drive drive[4];

  static const CommandFunc CommandTable[32];

  void CmdInvalid();
  void CmdSpecify();
  void CmdRecalibrate();
  void CmdSenceIntStatus();
  void CmdSenceDeviceStatus();
  void CmdSeek();
  void CmdReadData();
  void CmdReadDiagnostic();
  void CmdWriteData();
  void CmdReadID();
  void CmdWriteID();
  void CmdScanEqual();

 private:
  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
