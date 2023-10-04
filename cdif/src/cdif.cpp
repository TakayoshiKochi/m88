// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  CD-ROM インターフェースの実装
// ----------------------------------------------------------------------------
//  $Id: cdif.cpp,v 1.2 1999/10/10 01:39:00 cisc Exp $

#include "cdif.h"

#include <algorithm>

// #define LOGNAME "cdif"
#include "common/diag.h"

using namespace PC8801;

// ----------------------------------------------------------------------------
//  構築
//
CDIF::CDIF(const ID& id) : Device(id) {}

// ----------------------------------------------------------------------------
//  破棄
//
CDIF::~CDIF() = default;

// ----------------------------------------------------------------------------
//  初期化
//
bool CDIF::Init(IDMAAccess* dmac) {
  dmac_ = dmac;
  if (!cdrom_.Init())
    return false;
  if (!cd_.Init(&cdrom_, this, (void(Device::*)(int)) & CDIF::Done))
    return false;
  enable_ = false;
  active_ = false;
  return true;
}

bool CDIF::Enable(bool f) {
  enable_ = f;
  return true;
}

// ----------------------------------------------------------------------------
//  M88 のリセット
//  BASICMODE の bit 6 で判定
//
void IOCALL CDIF::SystemReset(uint32_t, uint32_t d) {
  Enable((d & 0x40) != 0);
}

// ----------------------------------------------------------------------------
//  リセット
//
void CDIF::Reset() {
  Log("Reset!\n");
  status_ = 0;
  data_ = 0;
  phase_ = kIdlePhase;

  play_mode_ = 0;
  addrs_ = 0;

  still_mode_ = 0;
  cd_.SendCommand(CDControl::kStop, true);
}

// ----------------------------------------------------------------------------
//  データ交換
//
void CDIF::DataOut() {
  int s = status_ & 0x38;
  status_ &= ~(0x40 | 0x38);
  switch (s) {
    case 0x00:  // データ送信モード
      *ptr_++ = data_;
      Log("(%.2x)", data_);

      if (--length_ > 0)
        status_ |= 0x00 | 0x40;
      else {
        Log("\n");
        ProcessCommand();
      }
      break;

    case 0x10:
      switch (phase_) {
        case kCmd2Phase:
          length_ = 1 + (data_ < 0x80 ? 5 : 9);
          phase_ = kParamPhase;
          ptr_ = cmdbuf_;
          Log("Command: ");

        case kParamPhase:
          Log("[%.2x]", data_);
          *ptr_++ = data_;
          if (--length_ > 0) {
            status_ |= 0x40 | 0x10;
          } else {
            Log(" - ");
            status_ &= ~0x38;
            phase_ = kExecPhase;
            ProcessCommand();
          }
          break;
      }
      break;
  }
}

void CDIF::DataIn() {
  int s = status_ & 0x38;
  status_ &= ~(0x40 | 0x38);
  switch (s) {
    case 0x08:  // データ受信モード
      if (length_-- > 0) {
        status_ |= 0x08 | 0x40;
        data_ = *ptr_++;
      } else
        ResultPhase(rslt_, stat_);
      break;

    case 0x18:  // 結果受信モード
      Log(">Status\n");
      data_ = 0;
      phase_ = kStatusPhase;
      status_ |= 0x38 | 0x40;
      break;

    case 0x38:  // 終了ステータス受信モード
      Log(">Idle\n");
      data_ = 0;
      phase_ = kIdlePhase;
      status_ = 0;
      break;
  }
}

void CDIF::SendPhase(int bytes, int r, int s) {
  rslt_ = r, stat_ = s;

  Log(">SendPhase\n");
  phase_ = kSendPhase;
  length_ = bytes - 1;
  status_ |= 0x08 | 0x40;
  ptr_ = databuf_;
  data_ = *ptr_++;
}

void CDIF::RecvPhase(int bytes) {
  Log(">RecvPhase\n");
  phase_ = kRecvPhase;
  length_ = bytes;
  status_ |= 0x00 | 0x40;
  ptr_ = databuf_;
}

void CDIF::ResultPhase(int res, int st) {
  Log(">Result\n");
  data_ = res;
  stat_ = st;
  status_ |= 0x18 | 0x40;
  phase_ = kResultPhase;
}

// ----------------------------------------------------------------------------
//
//
void CDIF::SendCommand(uint32_t a, uint32_t b, uint32_t c) {
  phase_ = kWaitPhase;
  cd_.SendCommand(a, b, c);
}

void CDIF::Done(int ret) {
  Log("[done(%d:%d:%d)]", cmdbuf_[0], phase_, ret);
  rslt_ = ret;
  if (phase_ == kWaitPhase)
    ProcessCommand();
}

// ----------------------------------------------------------------------------
//  コマンド処理
//
void CDIF::ProcessCommand() {
  switch (cmdbuf_[0]) {
    case 0x00:
      CheckDriveStatus();
      break;
    case 0x08:
      ReadSector();
      break;
    case 0x15:
      SetReadMode();
      break;

    case 0xd8:
      TrackSearch();
      break;
    case 0xd9:
      PlayStart();
      break;
    case 0xda:
      PlayStop();
      break;
    case 0xdd:
      ReadSubcodeQ();
      break;
    case 0xde:
      ReadTOC();
      break;

    default:
      Log("unknown\n");
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  セクタを読む
//
void CDIF::ReadSector() {
  switch (phase_) {
    int n;

    case kExecPhase:
      sector_ = (((cmdbuf_[1] << 8) + cmdbuf_[2]) << 8) + cmdbuf_[3];
      Log("Read Sector (%d)\n", sector_);
      //      statusdisplay.Show(90, 0, "Read Sector (%d)", sector);
      length_ = retry_count_ + 1;
      rslt_ = 0;

    case kWaitPhase:
      if (!rslt_) {
        if (length_-- > 0) {
          Log("(%2d) Read#%d\n", rslt_, retry_count_ - length_ + 1);
          SendCommand(read_mode_ ? CDControl::kRead2 : CDControl::kRead1, sector_,
                      (uint32_t)tmpbuf_);
          break;
        }
        ResultPhase(0, 0);
        break;
      }
      n = dmac_->RequestWrite(1, tmpbuf_, read_mode_ ? 2340 : 2048);
      Log("DMA: %d bytes\n", n);
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  セクタ読み込みモードの設定
//
void CDIF::SetReadMode() {
  switch (phase_) {
    case kExecPhase:
      Log("Set Read Mode (%d)\n", cmdbuf_[4]);
      RecvPhase(11);
      break;

    case kRecvPhase:
      retry_count_ = databuf_[10];
      read_mode_ = databuf_[4];
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  トラックサーチ
//
void CDIF::TrackSearch() {
  switch (phase_) {
    uint32_t addre;

    case kExecPhase:
      addrs_ = GetPlayAddress();
      //      addre = cmdbuf[1] & 1 ? cdrom.GetTrackInfo(0)->addr : addrs+1;
      addre = cdrom_.GetTrackInfo(0)->addr;

      Log("Track Search (%d - %d)\n", addrs_, addre);
      //      statusdisplay.Show(90, 0, "Search Track (%d)", addrs);
      SendCommand(CDControl::kPlayAudio, addrs_, addre);
      if (cmdbuf_[1] & 1)
        still_mode_ = 2;
      else
        still_mode_ = 0;
      break;

    case kWaitPhase:
      if (still_mode_ == 0) {
        still_mode_ = 2;
        SendCommand(CDControl::kPause);
        break;
      }
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//
//
void CDIF::PlayStart() {
  switch (phase_) {
    uint32_t addre;

    case kExecPhase:
      addre = GetPlayAddress();
      Log("Audio Play Start (%d - %d)\n", addrs_, addre);
      //      statusdisplay.Show(90, 0, "Play Audio (%d - %d)", addrs, addre);
      SendCommand(CDControl::kPlayAudio, addrs_, addre);
      break;

    case kWaitPhase:
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//
//
void CDIF::PlayStop() {
  switch (phase_) {
    case kExecPhase:
      addrs_ = cd_.GetTime();
      SendCommand(CDControl::kPause);
      //      statusdisplay.Show(90, 0, "Pause");
      still_mode_ = 1;
      break;

    case kWaitPhase:
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  サブコード読込
//
void CDIF::ReadSubcodeQ() {
  switch (phase_) {
    case kExecPhase:
      Log("Read Subcode-Q\n");
      SendCommand(CDControl::kReadSubCodeq, (uint32_t)tmpbuf_);
      break;

    case kWaitPhase:
      switch (tmpbuf_[1]) {
        case 0x11:
          databuf_[0] = 0;
          break;
        case 0x12:
          databuf_[0] = still_mode_;
          break;
        case 0x13:
          databuf_[0] = 3;
          break;
        default:
          databuf_[0] = 3;
          break;
      }
      databuf_[1] = tmpbuf_[5] & 0x0f;
      databuf_[2] = NtoBCD(tmpbuf_[6]);
      databuf_[3] = NtoBCD(tmpbuf_[7]);
      databuf_[4] = NtoBCD(tmpbuf_[13]);
      databuf_[5] = NtoBCD(tmpbuf_[14]);
      databuf_[6] = NtoBCD(tmpbuf_[15]);
      databuf_[7] = NtoBCD(tmpbuf_[9]);
      databuf_[8] = NtoBCD(tmpbuf_[10]);
      databuf_[9] = NtoBCD(tmpbuf_[11]);
      SendPhase(std::min(cmdbuf_[1], (uint8_t)10U), 0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  ドライブ状態の取得
//
void CDIF::CheckDriveStatus() {
  switch (phase_) {
    case kExecPhase:
      Log("Check Drive Status");
      SendCommand(CDControl::kCheckDisc);
      break;

    case kWaitPhase:
      Log("result : %d (%d)\n", rslt_, cdrom_.GetNumTracks());
      ResultPhase(0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  READ TOC
//
void CDIF::ReadTOC() {
  int t = 0;
  switch (phase_) {
    case kExecPhase:
      Log("READ TOC - ");

      switch (cmdbuf_[1]) {
        case 0x00:
          SendCommand(CDControl::kReadTOC);
          break;

        case 0x02:
          t = (cmdbuf_[2] / 16) * 10 + (cmdbuf_[2] & 0x0f);
        case 0x01:
          if (t <= cdrom_.GetNumTracks()) {
            const CDROM::Track* tr = cdrom_.GetTrackInfo(t);
            if (t)
              Log("Track %d(%p) ", t, tr);
            else
              Log("ReadOut");

            CDROM::MSF msf = cdrom_.ToMSF(tr->addr);
            databuf_[0] = msf.min;
            databuf_[1] = msf.sec;
            databuf_[2] = msf.frame;
            databuf_[3] = t ? tr->control & 0x0f : 0;
            Log(" : %8d/%.2x:%.2x.%.2x %.2x\n", tr->addr, msf.min, msf.sec, msf.frame,
                t ? tr->control : 0);
            SendPhase(4, 0, 0);
            break;
          }
          ResultPhase(0, 0);
          break;

        default:
          ResultPhase(0, 0);
          break;
      }
      break;

    case kWaitPhase:
      rslt_ = cdrom_.GetNumTracks();
      //      statusdisplay.Show(90, 0, "Read TOC - %d tracks", rslt);
      Log("GetNumTracks (%d)\n", rslt_);
      for (t = 0; t < rslt_; t++) {
        const CDROM::Track* tr = cdrom_.GetTrackInfo(t);
        Log("  %d: %d\n", t, tr->addr);
      }
      if (rslt_)
        databuf_[0] = 1, databuf_[1] = NtoBCD(rslt_);
      else
        databuf_[0] = 0, databuf_[1] = 0;
      databuf_[2] = 0;
      databuf_[3] = 0;
      SendPhase(4, 0, 0);
      break;
  }
}

// ----------------------------------------------------------------------------
//  再生コマンドのアドレスを取得
//
uint32_t CDIF::GetPlayAddress() {
  switch (cmdbuf_[9] & 0xc0) {
    CDROM::MSF msf;
    int t;

    case 0x00:
      return (((((cmdbuf_[2] << 8) + cmdbuf_[3]) << 8) + cmdbuf_[4]) << 8) + cmdbuf_[5];

    case 0x40:
      msf.min = cmdbuf_[2];
      msf.sec = cmdbuf_[3];
      msf.frame = cmdbuf_[4];
      return cdrom_.ToLBA(msf);

    case 0x80:
      t = BCDtoN(cmdbuf_[2]);
      if (0 < t && t <= cdrom_.GetNumTracks())
        return cdrom_.GetTrackInfo(t)->addr;
    default:
      return cdrom_.GetTrackInfo(0)->addr;
  }
}

// ----------------------------------------------------------------------------
//  I/O
//
void IOCALL CDIF::Out90(uint32_t, uint32_t d) {
  Log("O[90] <- %.2x\n", d);
  if (d & 1) {
    if (active_ && data_ == 0x81) {
      Log("Command_A\n");
      status_ |= 0x40 | 0x10 | 1;
      status_ &= ~(0x80 | 0x38);
      phase_ = kCmd1Phase;
    }
  } else {
    status_ &= ~1;
    if (phase_ == kCmd1Phase) {
      Log("Command_B\n");
      phase_ = kCmd2Phase;
      status_ = (status_ & ~0x78) | 0x80 | 0x50;
    }
  }
}

uint32_t IOCALL CDIF::In90(uint32_t) {
  //  Log("I[90] -> %.2x\n", status);
  return status_;
}

// ----------------------------------------------------------------------------
//  データポート
//
void IOCALL CDIF::Out91(uint32_t, uint32_t d) {
  //  Log("O[91] <- %.2x (DATA)\n", d);
  data_ = d;
  if (status_ & 0x80)
    DataOut();
}

uint32_t IOCALL CDIF::In91(uint32_t) {
  Log("I[91] -> %.2x\n", data_);
  uint32_t r = data_;
  if (status_ & 0x80)
    DataIn();
  return r;
}

void IOCALL CDIF::Out94(uint32_t, uint32_t d) {
  Log("O[94] <- %.2x\n", d);
  if (d & 0x80) {
    Reset();
  }
}

void IOCALL CDIF::Out97(uint32_t, uint32_t d) {
  //  cd.SendCommand(CDControl::playtrack, d);
  Log("O[97] <- %.2x\n", d);
}

void IOCALL CDIF::Out99(uint32_t, uint32_t d) {
  //  Log("O[99] <- %.2x\n", d);
}

void IOCALL CDIF::Out9f(uint32_t, uint32_t d) {
  //  cd.SendCommand(CDControl::readtoc);
  Log("O[9f] <- %.2x", d);
  if (enable_) {
    active_ = d & 1;
    Log("  CD-ROM drive %s.\n", active_ ? "activated" : "deactivated");
  }
}

uint32_t IOCALL CDIF::In92(uint32_t) {
  Log("I[92] -> %.2x\n", 0);
  return 0;
}

uint32_t IOCALL CDIF::In93(uint32_t) {
  Log("I[93] -> %.2x\n", 0);
  return 0;
}

uint32_t IOCALL CDIF::In96(uint32_t) {
  Log("I[96] -> %.2x\n", 0);
  return 0;
}

uint32_t IOCALL CDIF::In99(uint32_t) {
  Log("I[99] -> %.2x\n", 0);
  return 0;
}

uint32_t IOCALL CDIF::In9b(uint32_t) {
  //  Log("I[9b] -> %.2x\n", 0);
  return 60;
}

uint32_t IOCALL CDIF::In9d(uint32_t) {
  //  Log("I[9d] -> %.2x\n", 0);
  return 60;
}

// ---------------------------------------------------------------------------
//  再生モード
//
void IOCALL CDIF::Out98(uint32_t, uint32_t d) {
  Log("O[98] <- %.2x\n", d);
  play_mode_ = d;
}

uint32_t IOCALL CDIF::In98(uint32_t) {
  if (enable_)
    clk_ = ~clk_;

  uint32_t r = (clk_ & 0x80) | (play_mode_ & 0x7f);
  Log("I[98] -> %.2x\n", r);
  return r;
}

// ---------------------------------------------------------------------------
//  状態データのサイズ
//
uint32_t IFCALL CDIF::GetStatusSize() {
  return sizeof(Snapshot);
}

// ---------------------------------------------------------------------------
//  状態保存
//
bool IFCALL CDIF::SaveStatus(uint8_t* s) {
  Snapshot* ss = (Snapshot*)s;

  ss->rev = ssrev;
  ss->phase = phase_;
  ss->status = status_;
  ss->data = data_;
  ss->playmode = play_mode_;
  ss->retrycount = retry_count_;
  ss->stillmode = still_mode_;
  ss->rslt = rslt_;
  ss->sector = sector_;
  ss->ptr = ptr_ - cmdbuf_;
  ss->length = length_;
  ss->addrs = addrs_;

  memcpy(ss->buf, cmdbuf_, 16 + 16 + 2340);
  return true;
}

bool IFCALL CDIF::LoadStatus(const uint8_t* s) {
  const Snapshot* ss = (const Snapshot*)s;
  if (ss->rev != ssrev)
    return false;

  phase_ = ss->phase;
  status_ = ss->status;
  data_ = ss->data;
  play_mode_ = ss->playmode;
  retry_count_ = ss->retrycount;
  still_mode_ = ss->stillmode;
  rslt_ = ss->rslt;
  sector_ = ss->sector;
  ptr_ = cmdbuf_ + ss->ptr;
  length_ = ss->length;
  addrs_ = ss->addrs;

  memcpy(cmdbuf_, ss->buf, 16 + 16 + 2340);

  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor CDIF::descriptor = {indef, outdef};

const Device::OutFuncPtr CDIF::outdef[] = {
    static_cast<Device::OutFuncPtr>(&SystemReset), static_cast<Device::OutFuncPtr>(&Out90),
    static_cast<Device::OutFuncPtr>(&Out91),       static_cast<Device::OutFuncPtr>(&Out94),
    static_cast<Device::OutFuncPtr>(&Out97),       static_cast<Device::OutFuncPtr>(&Out98),
    static_cast<Device::OutFuncPtr>(&Out99),       static_cast<Device::OutFuncPtr>(&Out9f),
};

const Device::InFuncPtr CDIF::indef[] = {
    static_cast<Device::InFuncPtr>(&In90), static_cast<Device::InFuncPtr>(&In91),
    static_cast<Device::InFuncPtr>(&In92), static_cast<Device::InFuncPtr>(&In93),
    static_cast<Device::InFuncPtr>(&In96), static_cast<Device::InFuncPtr>(&In98),
    static_cast<Device::InFuncPtr>(&In99), static_cast<Device::InFuncPtr>(&In9b),
    static_cast<Device::InFuncPtr>(&In9d),
};
