// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: piccolo.h,v 1.2 2002/05/31 09:45:22 cisc Exp $

#pragma once

#include <windows.h>

#include "common/threadable.h"
#include "common/real_time_keeper.h"

#include <stdint.h>
#include <mutex>

//  遅延送信対応 ROMEO ドライバ
//
class PiccoloChip {
 public:
  PiccoloChip() = default;
  virtual ~PiccoloChip() = default;
  virtual int Init(uint32_t c) = 0;
  virtual void Reset(bool) = 0;
  virtual bool SetReg(uint32_t at, uint32_t addr, uint32_t data) = 0;
  virtual void SetChannelMask(uint32_t mask) = 0;
  virtual void SetVolume(int ch, int value) = 0;
};

enum PICCOLO_CHIPTYPE { PICCOLO_INVALID = 0, PICCOLO_YMF288, PICCOLO_YM2608 };

class Piccolo : public Threadable<Piccolo> {
 public:
  // constructor is protected
  virtual ~Piccolo() = default;

  enum PICCOLO_ERROR {
    PICCOLO_SUCCESS = 0,
    PICCOLOE_UNKNOWN = -32768,
    PICCOLOE_DLL_NOT_FOUND,
    PICCOLOE_ROMEO_NOT_FOUND,
    PICCOLOE_HARDWARE_NOT_AVAILABLE,
    PICCOLOE_HARDWARE_IN_USE,
    PICCOLOE_TIME_OUT_OF_RANGE,
    PICCOLOE_THREAD_ERROR,
  };

  static Piccolo* GetInstance() {
    std::call_once(once_, &Piccolo::InitHardware);
    return instance;
  }

  static void DeleteInstance();

  // 遅延バッファのサイズを設定
  bool SetLatencyBufferSize(uint32_t entry);

  // 遅延時間の最大値を設定
  // SetReg が呼び出されたとき、microsec 後以降のレジスタ書き込みを指示する at の値を指定した場合
  // 呼び出しは却下されるかもしれない。
  bool SetMaximumLatency(uint32_t microsec);

  // メソッド呼び出し時点での時間を渡す(単位は microsec)
  uint32_t GetCurrentTimeUS();

  //
  virtual int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc) = 0;
  virtual void SetReg(uint32_t addr, uint32_t data) = 0;

  int IsDriverBased() const { return avail_; }

  bool DrvSetReg(uint32_t at, uint32_t addr, uint32_t data);
  void DrvReset();
  void DrvRelease();

  void ThreadInit();
  bool ThreadLoop();

 protected:
  Piccolo() = default;

  struct Event {
    uint32_t at;
    uint32_t addr;
    uint32_t data;
  };

  static Piccolo* instance;

  virtual int Init();
  static void InitHardware();
  void CleanUp();

  RealTimeKeeper keeper_;
  std::mutex mtx_;

  bool Push(Event&);
  Event* Top();
  void Pop();

  Event* events_ = nullptr;
  int ev_read_ = 0;
  int ev_write_ = 0;
  int ev_entries_ = 0;

  int max_latency_us_ = 0;

  uint32_t addr_ = 0;
  uint32_t irq_ = 0;

  std::atomic<bool> active_ = false;
  static std::once_flag once_;

  int avail_ = 0;
};
