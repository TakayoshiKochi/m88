// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  uPD765A
// ---------------------------------------------------------------------------
//  $Id: fdc.cpp,v 1.20 2003/05/12 22:26:35 cisc Exp $

#include "pc88/fdc.h"

#include <stdlib.h>

#include <algorithm>

#include "common/io_bus.h"
#include "common/status.h"
#include "pc88/config.h"
#include "services/diskmgr.h"

// #define LOGNAME "fdc"
#include "common/diag.h"

namespace pc8801 {
FDC::FDC(const ID& id) : Device(id) {
  for (int i = 0; i < num_drives; i++) {
    drive_[i].cyrinder = 0;
    drive_[i].result = 0;
    drive_[i].hd = 0;
    drive_[i].dd = 1;
  }
}

FDC::~FDC() = default;

bool FDC::Init(services::DiskManager* dm, Scheduler* s, IOBus* b, int ip, int sp) {
  diskmgr_ = dm;
  scheduler_ = s;
  bus_ = b;

  pintr_ = ip;
  pfd_stat_ = sp;

  show_status_ = false;
  seek_state_ = 0;
  fd_stat_ = 0;
  disk_wait_ = true;

  if (!buffer_)
    buffer_ = std::make_unique<uint8_t[]>(0x4000);
  if (!buffer_)
    return false;
  memset(buffer_.get(), 0, 0x4000);

  Log("FDC LOG\n");
  Reset();
  return true;
}

// ---------------------------------------------------------------------------
//  設定反映
//
void FDC::ApplyConfig(const Config* cfg) {
  disk_wait_ = !(cfg->flag2 & Config::kFDDNoWait);
  show_status_ = (cfg->flags & Config::kShowFDCStatus) != 0;
}

// ---------------------------------------------------------------------------
//  ドライブ制御
//  0f4h/out    b5  b4  b3  b2  b1  b0
//              CLK DSI TD1 TD0 RV1 RV0
//
//  RVx:    ドライブのモード
//          0: 2D/2DD
//          1: 2HD
//
//  TDx:    ドライブのトラック密度
//          0: 48TPI (2D)
//          1: 96TPI (2DD/2HD)
//
void IOCALL FDC::DriveControl(uint32_t, uint32_t x) {
  int hdprev;
  Log("Drive control (%.2x) ", x);

  for (int d = 0; d < 2; d++) {
    hdprev = drive_[d].hd;
    drive_[d].hd = x & (1 << d) ? 0x80 : 0x00;
    drive_[d].dd = x & (4 << d) ? 0 : 1;
    if (hdprev != drive_[d].hd) {
      int bit = 4 << d;
      fd_stat_ = (fd_stat_ & ~bit) | (drive_[d].hd ? bit : 0);
      g_status_display->FDAccess(d, drive_[d].hd != 0, false);
    }
    Log("<2%c%c>", drive_[d].hd ? 'H' : 'D', drive_[d].dd ? ' ' : 'D');
  }
  if (pfd_stat_ >= 0)
    bus_->Out(pfd_stat_, fd_stat_);
  Log(">\n");
}

// ---------------------------------------------------------------------------
//  FDC::Intr
//  割り込み発生
//
inline void FDC::Intr(bool i) {
  bus_->Out(pintr_, i);
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL FDC::Reset(uint32_t, uint32_t) {
  Log("Reset\n");
  ShiftToIdlePhase();
  int_requested_ = false;
  Intr(false);
  scheduler_->DelEvent(this);
  DriveControl(0, 0);
  for (int d = 0; d < 2; d++)
    g_status_display->FDAccess(d, drive_[d].hd != 0, false);
  fd_stat_ &= ~3;
  if (pfd_stat_)
    bus_->Out(pfd_stat_, fd_stat_);
}

// ---------------------------------------------------------------------------
//  FDC::Status
//  ステータスレジスタ
//
//  MSB                         LSB
//  RQM DIO NDM CB  D3B D2B D1B D0B
//
//  CB  = idlephase 以外
//  NDM = E-Phase
//  DIO = 0 なら CPU->FDC (Put)  1 なら FDC->CPU (Get)
//  RQM = データの送信・受信の用意ができた
//
uint32_t IOCALL FDC::Status(uint32_t) {
  return seek_state_ | status_;
}

// ---------------------------------------------------------------------------
//  FDC::SetData
//  CPU から FDC にデータを送る
//
void IOCALL FDC::SetData(uint32_t, uint32_t d) {
  // 受け取れる状況かチェック
  if ((status_ & (S_RQM | S_DIO)) == S_RQM) {
    data_ = d;
    status_ &= ~S_RQM;
    Intr(false);

    switch (phase_) {
      // コマンドを受け取る
      case idlephase:
        Log("\n[%.2x] ", data);
        command_ = data_;
        (this->*CommandTable[command_ & 31])();
        break;

      // コマンドのパラメータを受け取る
      case commandphase:
        *bufptr_++ = data_;
        if (--count_)
          status_ |= S_RQM;
        else
          (this->*CommandTable[command_ & 31])();
        break;

      // E-Phase (転送中)
      case execwritephase:
        *bufptr_++ = data_;
        if (--count_) {
          status_ |= S_RQM, Intr(true);
        } else {
          status_ &= ~S_NDM;
          (this->*CommandTable[command_ & 31])();
        }
        break;

      // E-Phase (比較中)
      case execscanphase:
        if (data_ != 0xff) {
          if (((command_ & 31) == 0x11 && *bufptr_ != data_) ||
              ((command_ & 31) == 0x19 && *bufptr_ > data_) ||
              ((command_ & 31) == 0x1d && *bufptr_ < data_)) {
            // 条件に合わない
            result_ &= ~ST2_SH;
          }
        }
        bufptr_++;

        if (--count_) {
          status_ |= S_RQM, Intr(true);
        } else {
          status_ &= ~S_NDM;
          CmdScanEqual();
        }
        break;
      default:
        // nop;
        break;
    }
  }
}

// ---------------------------------------------------------------------------
//  FDC::GetData
//  FDC -> CPU
//
uint32_t IOCALL FDC::GetData(uint32_t) {
  if ((status_ & (S_RQM | S_DIO)) == (S_RQM | S_DIO)) {
    Intr(false);
    status_ &= ~S_RQM;

    switch (phase_) {
      // リザルト・フェイズ
      case resultphase:
        data_ = *bufptr_++;
        Log(" %.2x", data_);
        if (--count_)
          status_ |= S_RQM;
        else {
          Log(" }\n");
          ShiftToIdlePhase();
        }
        break;

      // E-Phase(転送中)
      case execreadphase:
        //          Log("ex= %d\n", scheduler->GetTime());
        //          Log("*");
        data_ = *bufptr_++;
        if (--count_) {
          status_ |= S_RQM, Intr(true);
        } else {
          Log("\n");
          status_ &= ~S_NDM;
          (this->*CommandTable[command_ & 31])();
        }
        break;
      default:
        // nop
        break;
    }
  }
  return data_;
}

// ---------------------------------------------------------------------------
//  TC (転送終了)
//
uint32_t IOCALL FDC::TC(uint32_t) {
  if (accept_tc_) {
    Log(" <TC>");
    prev_phase_ = phase_;
    phase_ = tcphase;
    accept_tc_ = false;
    (this->*CommandTable[command_ & 31])();
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  I-PHASE (コマンド待ち)
//
void FDC::ShiftToIdlePhase() {
  phase_ = idlephase;
  status_ = S_RQM;
  accept_tc_ = false;
  g_status_display->FDAccess(lit_drive_, drive_[lit_drive_].hd != 0, false);

  fd_stat_ &= ~(1 << lit_drive_);
  if (pfd_stat_ >= 0)
    bus_->Out(pfd_stat_, fd_stat_);

  Log("FD %d Off\n", lit_drive_);
}

// ---------------------------------------------------------------------------
//  C-PHASE (パラメータ待ち)
//
void FDC::ShiftToCommandPhase(int nbytes) {
  phase_ = commandphase;
  status_ = S_RQM | S_CB;
  accept_tc_ = false;

  bufptr_ = buffer_.get();
  count_ = nbytes;
  lit_drive_ = hdu_ & 1;
  Log("FD %d On\n", lit_drive_);
  g_status_display->FDAccess(lit_drive_, drive_[lit_drive_].hd != 0, true);

  fd_stat_ |= 1 << lit_drive_;
  if (pfd_stat_ >= 0)
    bus_->Out(pfd_stat_, fd_stat_);
}

// ---------------------------------------------------------------------------
//  E-PHASE
//
void FDC::ShiftToExecutePhase() {
  phase_ = executephase;
  (this->*CommandTable[command_ & 31])();
}

// ---------------------------------------------------------------------------
//  E-PHASE (FDC->CPU)
//
void FDC::ShiftToExecReadPhase(int nbytes, uint8_t* x) {
  phase_ = execreadphase;
  status_ = S_RQM | S_DIO | S_NDM | S_CB;
  accept_tc_ = true;
  Intr(true);

  bufptr_ = x ? x : buffer_.get();
  count_ = nbytes;
}

// ---------------------------------------------------------------------------
//  E-PHASE (CPU->FDC)
//
void FDC::ShiftToExecWritePhase(int nbytes) {
  phase_ = execwritephase;
  status_ = S_RQM | S_NDM | S_CB;
  accept_tc_ = true;
  Intr(true);

  bufptr_ = buffer_.get();
  count_ = nbytes;
}

// ---------------------------------------------------------------------------
//  E-PHASE (CPU->FDC, COMPARE)
//
void FDC::ShiftToExecScanPhase(int nbytes) {
  phase_ = execscanphase;
  status_ = S_RQM | S_NDM | S_CB;
  accept_tc_ = true;
  Intr(true);
  result_ = ST2_SH;

  bufptr_ = buffer_.get();
  count_ = nbytes;
}

// ---------------------------------------------------------------------------
//  R-PHASE
//
void FDC::ShiftToResultPhase(int nbytes) {
  phase_ = resultphase;
  status_ = S_RQM | S_CB | S_DIO;
  accept_tc_ = false;

  bufptr_ = buffer_.get();
  count_ = nbytes;
  Log("\t{");
}

// ---------------------------------------------------------------------------
//  R/W DATA 系 resultphase (ST0/ST1/ST2/C/H/R/N)

void FDC::ShiftToResultPhase7() {
  buffer_[0] = (result_ & 0xf8) | (hdue_ & 7);
  buffer_[1] = uint8_t(result_ >> 8);
  buffer_[2] = uint8_t(result_ >> 16);
  buffer_[3] = idr_.c;
  buffer_[4] = idr_.h;
  buffer_[5] = idr_.r;
  buffer_[6] = idr_.n;
  Intr(true);
  ShiftToResultPhase(7);
}

// ---------------------------------------------------------------------------
//  command や EOT を参考にレコード増加
//
bool FDC::IDIncrement() {
  //  Log("IDInc");
  if ((command_ & 19) == 17) {
    // Scan*Equal
    if ((dtl_ & 0xff) == 0x02)
      idr_.r++;
  }

  if (idr_.r++ != eot_) {
    //      Log("[1]\n");
    return true;
  }
  idr_.r = 1;
  if (command_ & 0x80) {
    hdu_ ^= 4;
    idr_.h ^= 1;
    //      Log("[2:%d]", hdu);
    if (idr_.h & 1) {
      //          Log("\n");
      return true;
    }
  }
  idr_.c++;
  //  Log("[3]\n");
  return false;
}

// ---------------------------------------------------------------------------
//  タイマー
//
void FDC::SetTimer(Phase p, int ticks) {
  t_phase_ = p;
  if (!disk_wait_)
    ticks = (ticks + 127) / 128;
  timer_handle_ = scheduler_->AddEventNS(ticks * kNanoSecsPerTick, this,
                                         static_cast<TimeFunc>(&FDC::PhaseTimer), p, false);
}

void FDC::DelTimer() {
  if (timer_handle_)
    scheduler_->DelEvent(timer_handle_);
  timer_handle_ = nullptr;
}

void IOCALL FDC::PhaseTimer(uint32_t p) {
  timer_handle_ = nullptr;
  phase_ = Phase(p);
  (this->*CommandTable[command_ & 31])();
}

// ---------------------------------------------------------------------------

const FDC::CommandFunc FDC::CommandTable[32] = {
    &FDC::CmdInvalid,        &FDC::CmdInvalid,  // 0
    &FDC::CmdReadDiagnostic, &FDC::CmdSpecify,     &FDC::CmdSenceDeviceStatus,
    &FDC::CmdWriteData,  // 4
    &FDC::CmdReadData,       &FDC::CmdRecalibrate, &FDC::CmdSenceIntStatus,
    &FDC::CmdWriteData,  // 8
    &FDC::CmdReadID,         &FDC::CmdInvalid,     &FDC::CmdReadData,
    &FDC::CmdWriteID,  // c
    &FDC::CmdInvalid,        &FDC::CmdSeek,        &FDC::CmdInvalid,
    &FDC::CmdScanEqual,  // 10
    &FDC::CmdInvalid,        &FDC::CmdInvalid,     &FDC::CmdInvalid,
    &FDC::CmdInvalid,  // 14
    &FDC::CmdInvalid,        &FDC::CmdInvalid,     &FDC::CmdInvalid,
    &FDC::CmdScanEqual,  // 18
    &FDC::CmdInvalid,        &FDC::CmdInvalid,     &FDC::CmdInvalid,
    &FDC::CmdScanEqual,  // 1c
    &FDC::CmdInvalid,        &FDC::CmdInvalid,
};

// ---------------------------------------------------------------------------
//  ReadData
//
void FDC::CmdReadData() {
  //  static int t0;
  switch (phase_) {
    case idlephase:
      Log((command_ & 31) == 12 ? "ReadDeletedData" : "ReadData ");
      ShiftToCommandPhase(8);  // パラメータは 8 個
      return;

    case commandphase:
      GetSectorParameters();
      SetTimer(executephase, 250 << std::min((uint8_t)7, idr_.n));
      return;

    case executephase:
      ReadData((command_ & 31) == 12, false);
      //      t0 = scheduler->GetTime();
      return;

    case execreadphase:
      //      Log("ex= %d\n", scheduler->GetTime()-t0);
      if (result_) {
        ShiftToResultPhase7();
        return;
      }
      if (!IDIncrement()) {
        SetTimer(timerphase, 20);
        return;
      }
      SetTimer(executephase, 250 << std::min((uint8_t)7, idr_.n));
      return;

    case tcphase:
      DelTimer();
      Log("\tTC at 0x%x byte\n", bufptr_ - buffer_.get());
      ShiftToResultPhase7();
      return;

    case timerphase:
      result_ = ST0_AT | ST1_EN;
      ShiftToResultPhase7();
      return;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  Scan*Equal
//
void FDC::CmdScanEqual() {
  switch (phase_) {
    case idlephase:
      Log("Scan");
      if ((command_ & 31) == 0x19) {
        Log("LowOr");
      } else if ((command_ & 31) == 0x1d) {
        Log("HighOr");
      }
      Log("Equal");
      ShiftToCommandPhase(9);
      return;

    case commandphase:
      GetSectorParameters();
      dtl_ = dtl_ | 0x100;  // STP パラメータ．DTL として無効な値を代入する．
      SetTimer(executephase, 200);
      return;

    case executephase:
      ReadData(false, true);
      return;

    case execscanphase:
      // Scan Data
      if (result_) {
        ShiftToResultPhase7();
        return;
      }
      phase_ = executephase;
      if (!IDIncrement()) {
        SetTimer(timerphase, 20);
        return;
      }
      SetTimer(executephase, 100);
      return;

    case tcphase:
      DelTimer();
      Log("\tTC at 0x%x byte\n", bufptr_ - buffer_.get());
      ShiftToResultPhase7();
      return;

    case timerphase:
      // 終了，みつかんなかった〜
      result_ = ST1_EN | ST2_SN;
      ShiftToResultPhase7();
      return;

    default:
      // nop
      break;
  }
}

void FDC::ReadData(bool deleted, bool scan) {
  Log("\tRead %.2x %.2x %.2x %.2x\n", idr_.c, idr_.h, idr_.r, idr_.n);
  if (show_status_)
    g_status_display->Show(85, 0, "%s (%d) %.2x %.2x %.2x %.2x", scan ? "Scan" : "Read", hdu_ & 3,
                           idr_.c, idr_.h, idr_.r, idr_.n);

  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
  result_ = CheckCondition(false);
  if (result_ & ST1_MA) {
    // ディスクが無い場合，100ms 後に再挑戦
    SetTimer(executephase, 10000);
    Log("Disk not mounted: Retry\n");
    return;
  }
  if (result_) {
    ShiftToResultPhase7();
    return;
  }

  uint32_t dr = hdu_ & 3;
  uint32_t flags = ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80);

  result_ = diskmgr_->GetFDU(dr)->ReadSector(flags, idr_, buffer_.get());

  if (deleted)
    result_ ^= ST2_CM;

  if ((result_ & ~ST2_CM) && !(result_ & ST2_DD)) {
    ShiftToResultPhase7();
    return;
  }
  if ((result_ & ST2_CM) && (command_ & 0x20))  // SKIP
  {
    SetTimer(timerphase, 1000);
    return;
  }
  int xbyte = idr_.n ? (0x80 << std::min((uint8_t)8, idr_.n)) : (std::min(dtl_, 0x80U));

  if (!scan)
    ShiftToExecReadPhase(xbyte);
  else
    ShiftToExecScanPhase(xbyte);
}

// ---------------------------------------------------------------------------
//  Seek
//
void FDC::CmdSeek() {
  switch (phase_) {
    case idlephase:
      Log("Seek ");
      ShiftToCommandPhase(2);
      break;

    case commandphase:
      Log("(%.2x %.2x)\n", buffer_[0], buffer_[1]);
      Seek(buffer_[0], buffer_[1]);

      ShiftToIdlePhase();
      break;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  Recalibrate
//
void FDC::CmdRecalibrate() {
  switch (phase_) {
    case idlephase:
      Log("Recalibrate ");
      ShiftToCommandPhase(1);
      break;

    case commandphase:
      Log("(%.2x)\n", buffer_[0]);

      Seek(buffer_[0] & 3, 0);
      ShiftToIdlePhase();
      break;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  指定のドライブをシークする
//
void FDC::Seek(uint32_t dr, uint32_t cy) {
  dr &= 3;

  cy <<= drive_[dr].dd;
  int seekcount = abs((int)cy - (int)drive_[dr].cyrinder);
  if (GetDeviceStatus(dr) & 0x80) {
    // FAULT
    Log("\tSeek on unconnected drive (%d)\n", dr);
    drive_[dr].result = (dr & 3) | ST0_SE | ST0_NR | ST0_AT;
    Intr(true);
    int_requested_ = true;
  } else {
    Log("Seek: %d -> %d (%d)\n", drive_[dr].cyrinder, cy, seekcount);
    drive_[dr].cyrinder = cy;
    seek_time_ = seekcount && disk_wait_ ? (400 * abs(seekcount) + 500) : 10;
    scheduler_->AddEventNS(seek_time_ * kNanoSecsPerTick, this,
                           static_cast<TimeFunc>(&FDC::SeekEvent), dr, false);
    seek_state_ |= 1 << dr;

    if (seek_time_ > 10) {
      fd_stat_ = (fd_stat_ & ~(1 << dr)) | 0x10;
      if (pfd_stat_ >= 0)
        bus_->Out(pfd_stat_, fd_stat_);
    }
  }
}

void IOCALL FDC::SeekEvent(uint32_t dr) {
  Log("\tSeek (%d) ", dr);
  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());

  if (seek_time_ > 1000) {
    fd_stat_ &= ~0x10;
    if (pfd_stat_ >= 0)
      bus_->Out(pfd_stat_, fd_stat_);
  }

  seek_time_ = 0;
  if (dr > num_drives || !diskmgr_->GetFDU(dr)->Seek(drive_[dr].cyrinder)) {
    drive_[dr].result = (dr & 3) | ST0_SE;
    Log("success.\n");
    //      g_status_display->Show(1000, 0, "0:%.2d 1:%.2d", drive[0].cyrinder, drive[1].cyrinder);
  } else {
    drive_[dr].result = (dr & 3) | ST0_SE | ST0_NR | ST0_AT;
    Log("failed.\n");
  }

  Intr(true);
  int_requested_ = true;
  seek_state_ &= ~(1 << dr);
}

// ---------------------------------------------------------------------------
//  Specify
//
void FDC::CmdSpecify() {
  switch (phase_) {
    case idlephase:
      Log("Specify ");
      ShiftToCommandPhase(2);
      break;

    case commandphase:
      Log("(%.2x %.2x)\n", buffer_[0], buffer_[1]);
      ShiftToIdlePhase();
      break;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  Invalid
//
void FDC::CmdInvalid() {
  Log("Invalid\n");
  buffer_[0] = uint8_t(ST0_IC);
  ShiftToResultPhase(1);
}

// ---------------------------------------------------------------------------
//  SenceIntState
//
void FDC::CmdSenceIntStatus() {
  if (int_requested_) {
    Log("SenceIntStatus ");
    int_requested_ = false;

    int i;
    for (i = 0; i < 4; i++) {
      if (drive_[i].result) {
        buffer_[0] = uint8_t(drive_[i].result);
        buffer_[1] = uint8_t(drive_[i].cyrinder >> drive_[i].dd);
        drive_[i].result = 0;
        ShiftToResultPhase(2);
        break;
      }
    }
    for (; i < 4; i++) {
      if (drive_[i].result)
        int_requested_ = true;
    }
  } else {
    Log("Invalid(SenceIntStatus)\n");
    buffer_[0] = uint8_t(ST0_IC);
    ShiftToResultPhase(1);
  }
}

// ---------------------------------------------------------------------------
//  SenceDeviceStatus
//
void FDC::CmdSenceDeviceStatus() {
  switch (phase_) {
    case idlephase:
      Log("SenseDeviceStatus ");
      ShiftToCommandPhase(1);
      return;

    case commandphase:
      Log("(%.2x) ", buffer_[0]);
      buffer_[0] = GetDeviceStatus(buffer_[0] & 3);
      ShiftToResultPhase(1);
      return;

    default:
      // nop
      break;
  }
}

uint32_t FDC::GetDeviceStatus(uint32_t dr) {
  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
  hdu_ = (hdu_ & ~3) | (dr & 3);
  if (dr < num_drives)
    return diskmgr_->GetFDU(dr)->SenseDeviceStatus() | dr;
  else
    return 0x80 | dr;
}

// ---------------------------------------------------------------------------
//  WriteData
//
void FDC::CmdWriteData() {
  switch (phase_) {
    case idlephase:
      Log((command_ & 31) == 9 ? "WriteDeletedData" : "WriteData ");
      ShiftToCommandPhase(8);
      return;

    case commandphase:
      GetSectorParameters();
      SetTimer(executephase, 200);
      return;

    case executephase:
      // FindID
      {
        std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
        result_ = CheckCondition(true);
        if (result_ & ST1_MA) {
          Log("Disk not mounted: Retry\n");
          SetTimer(executephase, 10000);  // retry
          return;
        }
        if (!result_) {
          uint32_t dr = hdu_ & 3;
          result_ = diskmgr_->GetFDU(dr)->FindID(
              ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80), idr_);
        }
        if (result_) {
          ShiftToResultPhase7();
          return;
        }
        int xbyte = 0x80 << std::min((uint8_t)8, idr_.n);
        if (!idr_.n) {
          xbyte = std::min(dtl_, 0x80U);
          memset(buffer_.get() + xbyte, 0, 0x80 - xbyte);
        }
        ShiftToExecWritePhase(xbyte);
      }
      return;

    case execwritephase:
      WriteData((command_ & 31) == 9);
      if (result_) {
        ShiftToResultPhase7();
        return;
      }
      phase_ = executephase;
      if (!IDIncrement()) {
        SetTimer(timerphase, 20);
        return;
      }
      SetTimer(executephase, 500);  // 実際は CRC, GAP の書き込みが終わるまで猶予があるはず
      return;

    case timerphase:
      // 次のセクタを処理しない
      result_ = ST0_AT | ST1_EN;
      ShiftToResultPhase7();
      return;

    case tcphase:
      DelTimer();
      Log("\tTC at 0x%x byte\n", bufptr_ - buffer_.get());
      if (prev_phase_ == execwritephase) {
        // 転送中？
        Log("flush");
        memset(bufptr_, 0, count_);
        WriteData((command_ & 31) == 9);
      }
      ShiftToResultPhase7();
      return;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  WriteID Execution
//
void FDC::WriteData(bool deleted) {
  Log("\twrite %.2x %.2x %.2x %.2x\n", idr_.c, idr_.h, idr_.r, idr_.n);
  if (show_status_)
    g_status_display->Show(85, 0, "Write (%d) %.2x %.2x %.2x %.2x", hdu_ & 3, idr_.c, idr_.h,
                           idr_.r, idr_.n);

  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
  result_ = CheckCondition(true);
  if (result_) {
    ShiftToResultPhase7();
    return;
  }

  uint32_t dr = hdu_ & 3;
  uint32_t flags = ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80);
  result_ = diskmgr_->GetFDU(dr)->WriteSector(flags, idr_, buffer_.get(), deleted);
}

// ---------------------------------------------------------------------------
//  ReadID
//
void FDC::CmdReadID() {
  switch (phase_) {
    case idlephase:
      Log("ReadID ");
      ShiftToCommandPhase(1);
      return;

    case commandphase:
      Log("(%.2x)", buffer_[0]);
      hdu_ = buffer_[0];

    case executephase:
      if (CheckCondition(false) & ST1_MA) {
        Log("Disk not mounted: Retry\n");
        SetTimer(executephase, 10000);
        return;
      }
      SetTimer(timerphase, 50);
      return;

    case timerphase:
      ReadID();
      return;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  ReadID Execution
//
void FDC::ReadID() {
  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
  result_ = CheckCondition(false);
  if (!result_) {
    uint32_t dr = hdu_ & 3;
    uint32_t flags = ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80);
    result_ = diskmgr_->GetFDU(dr)->ReadID(flags, &idr_);
    if (show_status_)
      g_status_display->Show(85, 0, "ReadID (%d:%.2x) %.2x %.2x %.2x %.2x", dr, flags, idr_.c,
                             idr_.h, idr_.r, idr_.n);
  }
  ShiftToResultPhase7();
}

// ---------------------------------------------------------------------------
//  WriteID
//
void FDC::CmdWriteID() {
  switch (phase_) {
    case idlephase:
      Log("WriteID ");
      ShiftToCommandPhase(5);
      return;

    case commandphase:
      Log("(%.2x %.2x %.2x %.2x %.2x)", buffer_[0], buffer_[1], buffer_[2], buffer_[3], buffer_[4]);

      wid_.idr = nullptr;
      hdu_ = buffer_[0];
      wid_.n = buffer_[1];
      eot_ = buffer_[2];
      wid_.gpl = buffer_[3];
      wid_.d = buffer_[4];

      if (!eot_) {
        // ???
        // buffer_ = bufptr_;
        SetTimer(timerphase, 10000);
        return;
      }
      ShiftToExecWritePhase(4 * eot_);
      return;

    case tcphase:
    case execwritephase:
      accept_tc_ = false;
      SetTimer(timerphase, 40000);
      return;

    case timerphase:
      wid_.idr = (IDR*)buffer_.get();
      wid_.sc = (bufptr_ - buffer_.get()) / 4;
      Log("sc:%d ", wid_.sc);

      WriteID();
      return;

    default:
      // nop
      break;
  }
}

// ---------------------------------------------------------------------------
//  WriteID Execution
//
void FDC::WriteID() {
#if defined(LOGNAME) && defined(_DEBUG)
  Log("\tWriteID  sc:%.2x N:%.2x\n", wid.sc, wid.n);
  for (int i = 0; i < wid.sc; i++) {
    Log("\t%.2x %.2x %.2x %.2x\n", wid.idr[i].c, wid.idr[i].h, wid.idr[i].r, wid.idr[i].n);
  }
#endif

  std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());

  result_ = CheckCondition(true);
  if (result_) {
    ShiftToResultPhase7();
    return;
  }

  uint32_t dr = hdu_ & 3;
  uint32_t flags = ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80);
  result_ = diskmgr_->GetFDU(dr)->WriteID(flags, &wid_);
  if (show_status_)
    g_status_display->Show(85, 0, "WriteID dr:%d tr:%d sec:%.2d N:%.2x", dr,
                           (drive_[dr].cyrinder >> drive_[dr].dd) * 2 + ((hdu_ >> 2) & 1), wid_.sc,
                           wid_.n);

  idr_.n = wid_.n;
  ShiftToResultPhase7();
}

// ---------------------------------------------------------------------------
//  ReadDiagnostic
//
void FDC::CmdReadDiagnostic() {
  switch (phase_) {
    int ct;
    case idlephase:
      Log("ReadDiagnostic ");
      ShiftToCommandPhase(8);  // パラメータは 8 個
      read_diag_ptr_ = 0;
      return;

    case commandphase:
      GetSectorParameters();
      SetTimer(executephase, 100);
      return;

    case executephase:
      ReadDiagnostic();
      if (result_ & ST0_AT) {
        ShiftToResultPhase7();
        return;
      }
      xbyte_ = idr_.n ? 0x80 << std::min((uint8_t)8, idr_.n) : std::min(dtl_, 0x80U);
      ct = std::min(uint32_t(read_diag_lim_ - read_diag_ptr_), xbyte_);
      read_diag_count_ = ct;
      ShiftToExecReadPhase(ct, read_diag_ptr_);
      read_diag_ptr_ += ct, xbyte_ -= ct;
      if (read_diag_ptr_ >= read_diag_lim_)
        read_diag_ptr_ = buffer_.get();
      return;

    case execreadphase:
      if (xbyte_ > 0) {
        ct = std::min(uint32_t(read_diag_lim_ - read_diag_ptr_), xbyte_);
        read_diag_count_ += ct;
        ShiftToExecReadPhase(ct, read_diag_ptr_);
        read_diag_ptr_ += ct, xbyte_ -= ct;
        if (read_diag_ptr_ >= read_diag_lim_)
          read_diag_ptr_ = buffer_.get();
        return;
      }
      if (!IDIncrement()) {
        SetTimer(timerphase, 20);
        return;
      }
      SetTimer(executephase, 100);
      return;

    case tcphase:
      DelTimer();
      Log("\tTC at 0x%x byte\n", read_diag_count_ - count);
      ShiftToResultPhase7();
      return;

    case timerphase:
      result_ = ST0_AT | ST1_EN;
      ShiftToResultPhase7();
      return;

    default:
      // nop
      break;
  }
}

void FDC::ReadDiagnostic() {
  Log("\tReadDiag ");
  if (!read_diag_ptr_) {
    std::lock_guard<std::mutex> lock(diskmgr_->GetMutex());
    result_ = CheckCondition(false);
    if (result_) {
      ShiftToResultPhase7();
      return;
    }

    if (result_ & ST1_MA) {
      // ディスクが無い場合，100ms 後に再挑戦
      Log("Disk not mounted: Retry\n");
      SetTimer(executephase, 10000);
      return;
    }

    uint32_t dr = hdu_ & 3;
    uint32_t flags = ((hdu_ >> 2) & 1) | (command_ & 0x40) | (drive_[dr].hd & 0x80);
    uint32_t size;
    int tr = (drive_[dr].cyrinder >> drive_[dr].dd) * 2 + ((hdu_ >> 2) & 1);
    g_status_display->Show(84, show_status_ ? 1000 : 2000, "ReadDiagnostic (Dr%d Tr%d)", dr, tr);

    result_ = diskmgr_->GetFDU(dr)->MakeDiagData(flags, buffer_.get(), &size);
    if (result_) {
      ShiftToResultPhase7();
      return;
    }
    read_diag_lim_ = buffer_.get() + size;
    Log("[0x%.4x]", size);
  }
  result_ = diskmgr_->GetFDU(hdu_ & 3)->ReadDiag(buffer_.get(), &read_diag_ptr_, idr_);
  Log(" (ptr=0x%.4x re=%d)\n", read_diag_ptr_ - buffer_.get(), result_);
}

// ---------------------------------------------------------------------------
//  Read/Write 操作が実行可能かどうかを確認
//
uint32_t FDC::CheckCondition(bool write) {
  uint32_t dr = hdu_ & 3;
  hdue_ = hdu_;
  if (dr >= num_drives) {
    return ST0_AT | ST0_NR;
  }
  if (!diskmgr_->GetFDU(dr)->IsMounted()) {
    return ST0_AT | ST1_MA;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  Read/Write Data 系のパラメータを得る
//
void FDC::GetSectorParameters() {
  Log("(%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x)\n", buffer_[0], buffer_[1], buffer_[2], buffer_[3],
      buffer_[4], buffer_[5], buffer_[6], buffer_[7]);

  hdu_ = hdue_ = buffer_[0];
  idr_.c = buffer_[1];
  idr_.h = buffer_[2];
  idr_.r = buffer_[3];
  idr_.n = buffer_[4];
  eot_ = buffer_[5];
  //  gpl   = buffer[6];
  dtl_ = buffer_[7];
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t FDC::GetStatusSize() {
  return sizeof(Snapshot);
}

bool FDC::SaveStatus(uint8_t* s) {
  auto* st = (Snapshot*)s;

  st->rev = ssrev;
  st->bufptr = bufptr_ ? bufptr_ - buffer_.get() : ~0;
  st->count = count_;
  st->status = status_;
  st->command = command_;
  st->data = data_;
  st->phase = phase_;
  st->prevphase = prev_phase_;
  st->t_phase = timer_handle_ ? t_phase_ : idlephase;
  st->int_requested = int_requested_;
  st->accepttc = accept_tc_;
  st->idr = idr_;
  st->hdu = hdu_;
  st->hdue = hdue_;
  st->dtl = dtl_;
  st->eot = eot_;
  st->seekstate = seek_state_;
  st->result = result_;
  st->readdiagptr = read_diag_ptr_ ? read_diag_ptr_ - buffer_.get() : ~0;
  st->readdiaglim = read_diag_lim_ - buffer_.get();
  st->xbyte = xbyte_;
  st->readdiagcount = read_diag_count_;
  st->wid = wid_;
  memcpy(st->buf, buffer_.get(), 0x4000);
  for (int d = 0; d < num_drives; d++)
    st->dr[d] = drive_[d];
  Log("save status  bufptr = %p\n", bufptr_);

  return true;
}

bool FDC::LoadStatus(const uint8_t* s) {
  const auto* st = (const Snapshot*)s;
  if (st->rev != ssrev)
    return false;

  bufptr_ = (st->bufptr == ~0) ? 0 : buffer_.get() + st->bufptr;
  count_ = st->count;
  status_ = st->status;
  command_ = st->command;
  data_ = st->data;
  phase_ = st->phase;
  prev_phase_ = st->prevphase;
  t_phase_ = st->t_phase;
  int_requested_ = st->int_requested;
  accept_tc_ = st->accepttc;
  idr_ = st->idr;
  hdu_ = st->hdu;
  hdue_ = st->hdue;
  dtl_ = st->dtl;
  eot_ = st->eot;
  seek_state_ = st->seekstate;
  result_ = st->result;
  read_diag_ptr_ = (st->readdiagptr == ~0) ? 0 : buffer_.get() + st->readdiagptr;
  read_diag_lim_ = buffer_.get() + st->readdiaglim;
  xbyte_ = st->xbyte;
  read_diag_count_ = st->readdiagcount;
  wid_ = st->wid;
  memcpy(buffer_.get(), st->buf, 0x4000);

  if ((command_ & 19) == 17)
    dtl_ |= 0x100;

  scheduler_->DelEvent(this);
  if (st->t_phase != idlephase)
    timer_handle_ =
        scheduler_->AddEventNS((disk_wait_ ? 100 : 10) * kNanoSecsPerTick, this,
                               static_cast<TimeFunc>(&FDC::PhaseTimer), st->t_phase, false);

  fd_stat_ = 0;
  for (int d = 0; d < num_drives; d++) {
    drive_[d] = st->dr[d];
    diskmgr_->GetFDU(d)->Seek(drive_[d].cyrinder);
    if (seek_state_ & (1 << d)) {
      scheduler_->AddEventNS((disk_wait_ ? 100 : 10) * kNanoSecsPerTick, this,
                             static_cast<TimeFunc>(&FDC::SeekEvent), d, false);
      fd_stat_ |= 0x10;
    }
    g_status_display->FDAccess(d, drive_[d].hd != 0, false);

    fd_stat_ |= drive_[d].hd ? 4 << d : 0;
  }
  if (pfd_stat_ >= 0)
    bus_->Out(pfd_stat_, fd_stat_);
  Log("load status  bufptr = %p\n", bufptr_);

  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor FDC::descriptor = {indef, outdef};

const Device::OutFuncPtr FDC::outdef[] = {static_cast<Device::OutFuncPtr>(&FDC::Reset),
                                          static_cast<Device::OutFuncPtr>(&FDC::SetData),
                                          static_cast<Device::OutFuncPtr>(&FDC::DriveControl),
                                          static_cast<Device::OutFuncPtr>(&FDC::MotorControl)};

const Device::InFuncPtr FDC::indef[] = {
    static_cast<Device::InFuncPtr>(&FDC::Status),
    static_cast<Device::InFuncPtr>(&FDC::GetData),
    static_cast<Device::InFuncPtr>(&FDC::TC),
};
}  // namespace pc8801