//  $Id: piccolo.h,v 1.2 2002/05/31 09:45:22 cisc Exp $

#ifndef incl_romeo_piccolo_h
#define incl_romeo_piccolo_h

#include "common/types.h"
#include "win32/timekeep.h"
#include "win32/critsect.h"

//  遅延送信対応 ROMEO ドライバ
//
class PiccoloChip {
 public:
  virtual ~PiccoloChip(){};
  virtual int Init(uint32_t c) = 0;
  virtual void Reset(bool) = 0;
  virtual bool SetReg(uint32_t at, uint32_t addr, uint32_t data) = 0;
  virtual void SetChannelMask(uint32_t mask) = 0;
  virtual void SetVolume(int ch, int value) = 0;
};

enum PICCOLO_CHIPTYPE { PICCOLO_INVALID = 0, PICCOLO_YMF288, PICCOLO_YM2608 };

class Piccolo {
 public:
  virtual ~Piccolo();

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

  static Piccolo* GetInstance();
  static void DeleteInstance();

  // 遅延バッファのサイズを設定
  bool SetLatencyBufferSize(uint32_t entry);

  // 遅延時間の最大値を設定
  // SetReg が呼び出されたとき、nanosec 後以降のレジスタ書き込みを指示する at の値を指定した場合
  // 呼び出しは却下されるかもしれない。
  bool SetMaximumLatency(uint32_t nanosec);

  // メソッド呼び出し時点での時間を渡す(単位は nanosec)
  uint32_t GetCurrentTime();

  //
  virtual int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc) = 0;
  virtual void SetReg(uint32_t addr, uint32_t data) = 0;

  int IsDriverBased() { return avail; }

 public:
  bool DrvSetReg(uint32_t at, uint32_t addr, uint32_t data);
  void DrvReset();
  void DrvRelease();

 protected:
  struct Event {
    uint32_t at;
    uint32_t addr;
    uint32_t data;
  };

  static Piccolo* instance;

  Piccolo();
  virtual int Init();
  void Cleanup();
  static uint32_t CALLBACK ThreadEntry(void* arg);
  uint32_t ThreadMain();

  TimeKeeper timekeeper;
  CriticalSection cs;

  bool Push(Event&);
  Event* Top();
  void Pop();

  Event* events;
  int evread;
  int evwrite;
  int eventries;

  int maxlatency;

  uint32_t addr;
  uint32_t irq;

  volatile bool shouldterminate;
  volatile bool active;
  HANDLE hthread;
  uint32_t idthread;

  int avail;
};

#endif  // incl_romeo_piccolo_h
