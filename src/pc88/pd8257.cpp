// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998.
// ---------------------------------------------------------------------------
//  PD8257 (uPD8257) のエミュレーション
// ---------------------------------------------------------------------------
//  $Id: pd8257.cpp,v 1.14 1999/10/10 01:47:09 cisc Exp $

#include "win32/headers.h"

#include <algorithm>

#include "pc88/pd8257.h"

// #define LOGNAME   "pd8257"
#include "common/diag.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築/消滅
// ---------------------------------------------------------------------------

PD8257::PD8257(const ID& id) : Device(id) {
  mread = 0, mrbegin = 0, mrend = 0;
  mwrite = 0, mwbegin = 0, mwend = 0;
  Reset();
}

PD8257::~PD8257() {}

// ---------------------------------------------------------------------------
//  接続
//  arg:    mem     接続するメモリ
//          addr    メモリのアドレス
//          length  メモリの長さ
//
bool PD8257::ConnectRd(uint8_t* mem, uint32_t addr, uint32_t length) {
  if (addr + length <= 0x10000) {
    mread = mem;
    mrbegin = addr;
    mrend = addr + length;
    Log("Connect(read) %.4x - %.4x bytes\n", addr, length);
    return true;
  } else
    mread = 0;
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
    mwrite = mem;
    mwbegin = addr;
    mwend = addr + length;
    Log("Connect(write) %.4x - %.4x bytes\n", addr, length);
    return true;
  } else
    mread = 0;
  return false;
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL PD8257::Reset(uint32_t, uint32_t) {
  stat.autoinit = 0;
  stat.ff = false;
  stat.enabled = 0;
  stat.status = 0;
  for (int i = 0; i < 4; i++) {
    stat.addr[i] = 0;
    stat.ptr[i] = 0;
    stat.count[i] = 0;
    stat.mode[i] = 0;
  }
}

// ---------------------------------------------------------------------------
//  アドレスレジスタをセット
//
void IOCALL PD8257::SetAddr(uint32_t d, uint32_t p) {
  int bank = (d / 2) & 3;
  if (!stat.ff)
    stat.ptr[bank] = (stat.ptr[bank] & 0xff00) | p;
  else
    stat.ptr[bank] = (stat.ptr[bank] & 0x00ff) | (p << 8);
  if (stat.autoinit && bank == 2)
    stat.ptr[3] = stat.ptr[2];

  stat.ff = !stat.ff;
  Log("Bank %d: addr  = %.4x\n", bank, stat.ptr[bank]);
}

// ---------------------------------------------------------------------------
//  カウンタレジスタをセット
//
void IOCALL PD8257::SetCount(uint32_t d, uint32_t p) {
  int bank = (d / 2) & 3;
  if (!stat.ff)
    stat.count[bank] = (stat.count[bank] & 0xff00) | p;
  else {
    stat.mode[bank] = p & 0xc0;
    stat.count[bank] = (stat.count[bank] & 0x00ff) | ((p & 0x3f) << 8);
  }
  if (stat.autoinit && bank == 2)
    stat.count[3] = stat.count[2], stat.mode[3] = stat.mode[3];
  stat.ff = !stat.ff;
  Log("Bank %d: count = %.4x  flag = %.4x\n", bank, stat.count[bank] & 0x3fff, stat.mode[bank]);
}

// ---------------------------------------------------------------------------
//  モードレジスタをセット
//
void IOCALL PD8257::SetMode(uint32_t, uint32_t d) {
  Log("Mode: %.2x\n", d);
  stat.autoinit = (d & 0x80) != 0;

  uint8_t pe = stat.enabled;
  stat.enabled = (uint8_t)(d & 15);

  stat.status &= ~stat.enabled;
  //  for (int i=0; i<4; i++)
  {
    //      if (!(pe & (1 << i)) && (d & (1 << i)))
    //          stat.ptr[i] = stat.addr[i];
  }
  stat.ff = false;
}

// ---------------------------------------------------------------------------
//  アドレスレジスタを読み込む
//
uint32_t IOCALL PD8257::GetAddr(uint32_t p) {
  int bank = (p / 2) & 3;
  stat.ff = !stat.ff;
  if (stat.ff)
    return stat.ptr[bank] & 0xff;
  else
    return (stat.ptr[bank] >> 8) & 0xff;
}

// ---------------------------------------------------------------------------
//  カウンタレジスタを読み込む
//
uint32_t IOCALL PD8257::GetCount(uint32_t p) {
  int bank = (p / 2) & 3;
  stat.ff = !stat.ff;
  if (stat.ff)
    return stat.count[bank] & 0xff;
  else
    return ((stat.count[bank] >> 8) & 0x3f) | stat.mode[bank];
}

// ---------------------------------------------------------------------------
//  ステータスを取得
//
inline uint32_t IOCALL PD8257::GetStatus(uint32_t) {
  //  stat.ff = false;
  return stat.status;
}

// ---------------------------------------------------------------------------
//  PD8257 を通じてメモリを読み込む
//  arg:bank    DMA バンクの番号
//      data    読み込むデータのポインタ
//      nbytes  転送サイズ
//  ret:        転送できたサイズ
//
uint32_t IFCALL PD8257::RequestRead(uint32_t bank, uint8_t* data, uint32_t nbytes) {
  uint32_t n = nbytes;
  Log("Request ");
  if ((stat.enabled & (1 << bank)) && !(stat.mode[bank] & 0x40)) {
    while (n > 0) {
      uint32_t size = std::min(n, (uint32_t)stat.count[bank] + 1);
      if (!size)
        break;

      if (mread && mrbegin <= stat.ptr[bank] && stat.ptr[bank] < mrend) {
        // 存在するメモリのアクセス
        size = std::min(size, mrend - stat.ptr[bank]);
        memcpy(data, mread + stat.ptr[bank] - mrbegin, size);
        Log("READ ch[%d] (%.4x - %.4x bytes)\n", bank, stat.ptr[bank], size);
      } else {
        // 存在しないメモリへのアクセス
        if (stat.ptr[bank] - mrbegin)
          size = std::min(size, mrbegin - stat.ptr[bank]);
        else
          size = std::min(size, 0x10000 - stat.ptr[bank]);

        memset(data, 0xff, size);
      }

      stat.ptr[bank] = (stat.ptr[bank] + size) & 0xffff;
      stat.count[bank] -= size;
      if (stat.count[bank] < 0) {
        if (bank == 2 && stat.autoinit) {
          stat.ptr[2] = stat.ptr[3];
          stat.count[2] = stat.count[3];
          Log("DMA READ: Bank%d auto init (%.4x:%.4x).\n", bank, stat.ptr[2], stat.count[2] + 1);
        } else {
          stat.status |= 1 << bank;  // TC
          Log("DMA READ: Bank%d end transmittion.\n", bank);
        }
      }
      n -= size;
    }
  } else
    Log("\n");

  return nbytes - n;
}

// ---------------------------------------------------------------------------
//  PD8257 を通じてメモリに書き込む
//  arg:bank    DMA バンクの番号
//      data    書き込むデータのポインタ
//      nbytes  転送サイズ
//  ret:        転送できたサイズ
//
uint32_t IFCALL PD8257::RequestWrite(uint32_t bank, uint8_t* data, uint32_t nbytes) {
  uint32_t n = nbytes;
  if ((stat.enabled & (1 << bank)) && !(stat.mode[bank] & 0x80)) {
    while (n > 0) {
      uint32_t size = std::min(n, (uint32_t)stat.count[bank] + 1);
      if (!size)
        break;

      if (mwrite && mwbegin <= stat.ptr[bank] && stat.ptr[bank] < mwend) {
        // 存在するメモリのアクセス
        size = std::min(size, mwend - stat.ptr[bank]);
        memcpy(mwrite + stat.ptr[bank] - mwbegin, data, size);
        Log("WRIT ch[%d] (%.4x - %.4x bytes)\n", bank, stat.ptr[bank], size);
      } else {
        // 存在しないメモリへのアクセス
        if (stat.ptr[bank] - mwbegin)
          size = std::min(size, mwbegin - stat.ptr[bank]);
        else
          size = std::min(size, 0x10000 - stat.ptr[bank]);
      }

      stat.ptr[bank] = (stat.ptr[bank] + size) & 0xffff;
      stat.count[bank] -= size;
      if (stat.count[bank] < 0) {
        if (bank == 2 && stat.autoinit) {
          stat.ptr[2] = stat.ptr[3];
          stat.count[2] = stat.count[3];
          Log("DMA WRITE: Bank%d auto init (%.4x:%.4x).\n", bank, stat.ptr[2], stat.count[2] + 1);
        } else {
          stat.status |= 1 << bank;  // TC
          Log("DMA WRITE: Bank%d end transmittion.\n", bank);
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
uint32_t IFCALL PD8257::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL PD8257::SaveStatus(uint8_t* s) {
  Status* st = (Status*)s;
  *st = stat;
  st->rev = ssrev;
  return true;
}

bool IFCALL PD8257::LoadStatus(const uint8_t* s) {
  const Status* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;
  stat = *st;
  return true;
}

// ---------------------------------------------------------------------------
//  Device descriptor
//
const Device::Descriptor PD8257::descriptor = {indef, outdef};

const Device::OutFuncPtr PD8257::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Reset),
    static_cast<Device::OutFuncPtr>(&SetAddr),
    static_cast<Device::OutFuncPtr>(&SetCount),
    static_cast<Device::OutFuncPtr>(&SetMode),
};

const Device::InFuncPtr PD8257::indef[] = {
    static_cast<Device::InFuncPtr>(&GetAddr),
    static_cast<Device::InFuncPtr>(&GetCount),
    static_cast<Device::InFuncPtr>(&GetStatus),
};
