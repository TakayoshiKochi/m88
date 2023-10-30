// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998.
// ---------------------------------------------------------------------------
//  PD8257 (uPD8257) のエミュレーション
// ---------------------------------------------------------------------------
//  $Id: pd8257.cpp,v 1.14 1999/10/10 01:47:09 cisc Exp $

#include "pc88/pd8257.h"

#include <algorithm>

// #define LOGNAME   "pd8257"
#include "common/diag.h"

namespace pc8801 {
PD8257::PD8257(const ID& id) : Device(id) {
  Reset();
}

PD8257::~PD8257() = default;

// ---------------------------------------------------------------------------
//  接続
//  arg:    mem     接続するメモリ
//          addr    メモリのアドレス
//          length  メモリの長さ
//
bool PD8257::ConnectRd(uint8_t* mem, uint32_t addr, uint32_t length) {
  if (addr + length <= 0x10000) {
    mread_ = mem;
    mrbegin_ = addr;
    mrend_ = addr + length;
    Log("Connect(read) %.4x - %.4x bytes\n", addr, length);
    return true;
  } else {
    mread_ = nullptr;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  接続
//  arg:    mem     接続するメモリ
//          addr    メモリのアドレス
//          length  メモリの長さ
//
bool PD8257::ConnectWr(uint8_t* mem, uint32_t addr, uint32_t length) {
  if (addr + length <= 0x10000) {
    mwrite_ = mem;
    mwbegin_ = addr;
    mwend_ = addr + length;
    Log("Connect(write) %.4x - %.4x bytes\n", addr, length);
    return true;
  } else {
    mread_ = nullptr;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL PD8257::Reset(uint32_t, uint32_t) {
  stat_.autoinit = false;
  stat_.ff = false;
  stat_.enabled = 0;
  stat_.status = 0;
  for (int i = 0; i < 4; i++) {
    stat_.addr[i] = 0;
    stat_.ptr[i] = 0;
    stat_.count[i] = 0;
    stat_.mode[i] = 0;
  }
}

// ---------------------------------------------------------------------------
//  アドレスレジスタをセット
//
void IOCALL PD8257::SetAddr(uint32_t d, uint32_t p) {
  int bank = (d / 2) & 3;
  if (!stat_.ff)
    stat_.ptr[bank] = (stat_.ptr[bank] & 0xff00) | p;
  else
    stat_.ptr[bank] = (stat_.ptr[bank] & 0x00ff) | (p << 8);
  if (stat_.autoinit && bank == 2)
    stat_.ptr[3] = stat_.ptr[2];

  stat_.ff = !stat_.ff;
  Log("Bank %d: addr  = %.4x\n", bank, stat_.ptr[bank]);
}

// ---------------------------------------------------------------------------
//  カウンタレジスタをセット
//
void IOCALL PD8257::SetCount(uint32_t d, uint32_t p) {
  int bank = (d / 2) & 3;
  if (!stat_.ff)
    stat_.count[bank] = (stat_.count[bank] & 0xff00) | p;
  else {
    stat_.mode[bank] = p & 0xc0;
    stat_.count[bank] = (stat_.count[bank] & 0x00ff) | ((p & 0x3f) << 8);
  }
  if (stat_.autoinit && bank == 2)
    stat_.count[3] = stat_.count[2], stat_.mode[3] = stat_.mode[3];
  stat_.ff = !stat_.ff;
  Log("Bank %d: count = %.4x  flag = %.4x\n", bank, stat_.count[bank] & 0x3fff, stat_.mode[bank]);
}

// ---------------------------------------------------------------------------
//  モードレジスタをセット
//
void IOCALL PD8257::SetMode(uint32_t, uint32_t d) {
  Log("Mode: %.2x\n", d);
  stat_.autoinit = (d & 0x80) != 0;

  uint8_t pe = stat_.enabled;
  stat_.enabled = (uint8_t)(d & 15);

  stat_.status &= ~stat_.enabled;
  //  for (int i=0; i<4; i++)
  {
    //      if (!(pe & (1 << i)) && (d & (1 << i)))
    //          stat.ptr[i] = stat.addr[i];
  }
  stat_.ff = false;
}

// ---------------------------------------------------------------------------
//  アドレスレジスタを読み込む
//
uint32_t IOCALL PD8257::GetAddr(uint32_t p) {
  int bank = (p / 2) & 3;
  stat_.ff = !stat_.ff;
  if (stat_.ff)
    return stat_.ptr[bank] & 0xff;
  else
    return (stat_.ptr[bank] >> 8) & 0xff;
}

// ---------------------------------------------------------------------------
//  カウンタレジスタを読み込む
//
uint32_t IOCALL PD8257::GetCount(uint32_t p) {
  int bank = (p / 2) & 3;
  stat_.ff = !stat_.ff;
  if (stat_.ff)
    return stat_.count[bank] & 0xff;
  else
    return ((stat_.count[bank] >> 8) & 0x3f) | stat_.mode[bank];
}

// ---------------------------------------------------------------------------
//  ステータスを取得
//
inline uint32_t IOCALL PD8257::GetStatus(uint32_t) {
  //  stat.ff = false;
  return stat_.status;
}

// ---------------------------------------------------------------------------
//  PD8257 を通じてメモリを読み込む
//  arg:bank    DMA バンクの番号
//      data    読み込むデータのポインタ
//      nbytes  転送サイズ
//  ret:        転送できたサイズ
//
uint32_t PD8257::RequestRead(uint32_t bank, uint8_t* dest, uint32_t nbytes) {
  uint32_t n = nbytes;
  Log("Request ");
  if ((stat_.enabled & (1 << bank)) && !(stat_.mode[bank] & 0x40)) {
    while (n > 0) {
      uint32_t size = std::min(n, (uint32_t)stat_.count[bank] + 1);
      if (!size)
        break;

      if (mread_ && mrbegin_ <= stat_.ptr[bank] && stat_.ptr[bank] < mrend_) {
        // 存在するメモリのアクセス
        size = std::min(size, mrend_ - stat_.ptr[bank]);
        memcpy(dest, mread_ + stat_.ptr[bank] - mrbegin_, size);
        Log("READ ch[%d] (%.4x - %.4x bytes)\n", bank, stat_.ptr[bank], size);
      } else {
        // 存在しないメモリへのアクセス
        if (stat_.ptr[bank] - mrbegin_)
          size = std::min(size, mrbegin_ - stat_.ptr[bank]);
        else
          size = std::min(size, 0x10000 - stat_.ptr[bank]);

        memset(dest, 0xff, size);
      }

      stat_.ptr[bank] = (stat_.ptr[bank] + size) & 0xffff;
      stat_.count[bank] -= size;
      if (stat_.count[bank] < 0) {
        if (bank == 2 && stat_.autoinit) {
          Reload();
          Log("DMA READ: Bank%d auto init (%.4x:%.4x).\n", bank, stat_.ptr[2], stat_.count[2] + 1);
        } else {
          stat_.status |= 1 << bank;  // TC
          Log("DMA READ: Bank%d end transmission.\n", bank);
        }
      }
      n -= size;
    }
  } else
    Log("\n");

  return nbytes - n;
}

void PD8257::Reload() {
  stat_.ptr[2] = stat_.ptr[3];
  stat_.count[2] = stat_.count[3];
}

// ---------------------------------------------------------------------------
//  PD8257 を通じてメモリに書き込む
//  arg:bank    DMA バンクの番号
//      data    書き込むデータのポインタ
//      nbytes  転送サイズ
//  ret:        転送できたサイズ
//
uint32_t PD8257::RequestWrite(uint32_t bank, uint8_t* data, uint32_t nbytes) {
  uint32_t n = nbytes;
  if ((stat_.enabled & (1 << bank)) && !(stat_.mode[bank] & 0x80)) {
    while (n > 0) {
      uint32_t size = std::min(n, (uint32_t)stat_.count[bank] + 1);
      if (!size)
        break;

      if (mwrite_ && mwbegin_ <= stat_.ptr[bank] && stat_.ptr[bank] < mwend_) {
        // 存在するメモリのアクセス
        size = std::min(size, mwend_ - stat_.ptr[bank]);
        memcpy(mwrite_ + stat_.ptr[bank] - mwbegin_, data, size);
        Log("WRIT ch[%d] (%.4x - %.4x bytes)\n", bank, stat_.ptr[bank], size);
      } else {
        // 存在しないメモリへのアクセス
        if (stat_.ptr[bank] - mwbegin_)
          size = std::min(size, mwbegin_ - stat_.ptr[bank]);
        else
          size = std::min(size, 0x10000 - stat_.ptr[bank]);
      }

      stat_.ptr[bank] = (stat_.ptr[bank] + size) & 0xffff;
      stat_.count[bank] -= size;
      if (stat_.count[bank] < 0) {
        if (bank == 2 && stat_.autoinit) {
          stat_.ptr[2] = stat_.ptr[3];
          stat_.count[2] = stat_.count[3];
          Log("DMA WRITE: Bank%d auto init (%.4x:%.4x).\n", bank, stat_.ptr[2], stat_.count[2] + 1);
        } else {
          stat_.status |= 1 << bank;  // TC
          Log("DMA WRITE: Bank%d end transmission.\n", bank);
        }
      }
      n -= size;
    }
  }
  return nbytes - n;
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t PD8257::GetStatusSize() {
  return sizeof(Status);
}

bool PD8257::SaveStatus(uint8_t* s) {
  auto* st = reinterpret_cast<Status*>(s);
  *st = stat_;
  st->rev = ssrev;
  return true;
}

bool PD8257::LoadStatus(const uint8_t* s) {
  const auto* st = reinterpret_cast<const Status*>(s);
  if (st->rev != ssrev)
    return false;
  stat_ = *st;
  return true;
}

// ---------------------------------------------------------------------------
//  Device descriptor
//
const Device::Descriptor PD8257::descriptor = {indef, outdef};

const Device::OutFuncPtr PD8257::outdef[] = {
    static_cast<Device::OutFuncPtr>(&PD8257::Reset),
    static_cast<Device::OutFuncPtr>(&PD8257::SetAddr),
    static_cast<Device::OutFuncPtr>(&PD8257::SetCount),
    static_cast<Device::OutFuncPtr>(&PD8257::SetMode),
};

const Device::InFuncPtr PD8257::indef[] = {
    static_cast<Device::InFuncPtr>(&PD8257::GetAddr),
    static_cast<Device::InFuncPtr>(&PD8257::GetCount),
    static_cast<Device::InFuncPtr>(&PD8257::GetStatus),
};
}  // namespace pc8801