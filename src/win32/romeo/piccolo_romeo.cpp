//  $Id: piccolo.cpp,v 1.3 2003/04/22 13:16:36 cisc Exp $

#include "win32/romeo/piccolo_romeo.h"

#include "win32/romeo/romeo.h"
#include "win32/status_bar_win.h"

// #define LOGNAME "piccolo"
#include "common/diag.h"

struct PCIDRV {
  HMODULE mod;
  bool reserve;
  PCIFINDDEV finddev;
  PCICFGREAD32 read32;
  PCIMEMWR8 out8;
  PCIMEMWR16 out16;
  PCIMEMWR32 out32;
  PCIMEMRD8 in8;
  PCIMEMRD16 in16;
  PCIMEMRD32 in32;
};

#define ROMEO_TPTR(member) ((intptr_t) & (((PCIDRV *)nullptr)->member))

struct DLLPROCESS {
  const char *symbol;
  intptr_t addr;
};

static const DLLPROCESS dllproc[] = {
    {FN_PCIFINDDEV, ROMEO_TPTR(finddev)}, {FN_PCICFGREAD32, ROMEO_TPTR(read32)},
    {FN_PCIMEMWR8, ROMEO_TPTR(out8)},     {FN_PCIMEMWR16, ROMEO_TPTR(out16)},
    {FN_PCIMEMWR32, ROMEO_TPTR(out32)},   {FN_PCIMEMRD8, ROMEO_TPTR(in8)},
    {FN_PCIMEMRD16, ROMEO_TPTR(in16)},    {FN_PCIMEMRD32, ROMEO_TPTR(in32)}};

static PCIDRV pcidrv = {
    nullptr,
};

class ChipIF : public PiccoloChip {
 public:
  explicit ChipIF(Piccolo_Romeo *p) { pic = p; }
  ~ChipIF() override { Mute(); }
  int Init(uint32_t param) override {
    pic->Reset();
    return true;
  }
  void Reset(bool opna) override {
    pic->DrvReset();
    pic->Reset();
  }
  bool SetReg(uint32_t at, uint32_t addr, uint32_t data) override {
    return pic->DrvSetReg(at, addr, data);
  }
  void SetChannelMask(uint32_t mask) override {}
  void SetVolume(int ch, int value) override {}

 private:
  Piccolo_Romeo *pic;

  void Mute() {
    Log("YMF288::Mute()\n");
    pic->SetReg(0x07, 0x3f);
    for (uint32_t r = 0x40; r < 0x4f; r++) {
      if (~r & 3) {
        pic->SetReg(0x000 + r, 0x7f);
        pic->SetReg(0x100 + r, 0x7f);
      }
    }
  }
};

static bool LoadDLL() {
  if (!pcidrv.mod) {
    pcidrv.mod = LoadLibrary(PCIDEBUG_DLL);
    if (!pcidrv.mod)
      return false;

    FARPROC proc;
    for (auto i : dllproc) {
      proc = GetProcAddress(pcidrv.mod, i.symbol);
      *(DWORD_PTR *)(((BYTE *)&pcidrv) + i.addr) = (DWORD_PTR)proc;
      if (!proc)
        break;
    }
    if (!proc) {
      FreeLibrary(pcidrv.mod);
      pcidrv.mod = nullptr;
    }
  }
  return pcidrv.mod != nullptr;
}

static void FreeDLL() {
  if (pcidrv.mod) {
    ::FreeLibrary(pcidrv.mod);
    pcidrv.mod = nullptr;
  }
}

// ---------------------------------------------------------------------------
//
//
Piccolo_Romeo::Piccolo_Romeo() : Piccolo() {}

Piccolo_Romeo::~Piccolo_Romeo() {
  CleanUp();
  FreeDLL();
}

int Piccolo_Romeo::Init() {
  {
    // DLL 用意
    Log("LoadDLL\n");
    if (!LoadDLL())
      return PICCOLOE_DLL_NOT_FOUND;

    avail_ = 1;

    // ROMEO の存在確認
    // デバイスを探す
    Log("FindDevice\n");
    uint32_t id;
    id = pcidrv.finddev(0x6809, 0x8121, 0);
    if (id & 0xff)
      id = pcidrv.finddev(0x6809, 0x2151, 0);
    Log(" ID = %.8x\n", id);
    if (id & 0xff)
      return PICCOLOE_ROMEO_NOT_FOUND;

    // ROMEO はありそうだが、デバイスは？
    id >>= 16;
    addr_ = pcidrv.read32(id, ROMEO_BASEADDRESS1);
    irq_ = pcidrv.read32(id, ROMEO_PCIINTERRUPT) & 0xff;
    Log(" ADDR = %.8x\n", addr_);
    Log(" IRQ  = %.8x\n", irq_);

    if (!addr_)
      return PICCOLOE_ROMEO_NOT_FOUND;
    addr_ += ROMEO_YMF288BASE;
  }
  Reset();

  Piccolo::Init();

  return PICCOLO_SUCCESS;
}

int Piccolo_Romeo::GetChip(PICCOLO_CHIPTYPE type, PiccoloChip **pc) {
  *pc = nullptr;
  Log("GetChip %d\n", type);
  if (type != PICCOLO_YMF288)
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;

  Log(" Type: YMF288\n");
  if (pcidrv.reserve) {
    return PICCOLOE_HARDWARE_IN_USE;
  }
  Log(" allocated\n");
  pcidrv.reserve = true;
  *pc = new ChipIF(this);
  return PICCOLO_SUCCESS;
}

// ---------------------------------------------------------------------------

void Piccolo_Romeo::Reset() {
  Log("YMF288::Reset()\n");
  Mute();
  if (avail_) {
    pcidrv.out32(addr_ + CTRL, 0x00);
    Sleep(150);
    pcidrv.out32(addr_ + CTRL, 0x80);
    Sleep(150);
  }
}

bool Piccolo_Romeo::IsBusy() {
  if (avail_)
    return (pcidrv.in8(addr_ + ADDR0) & 0x80) != 0;
  return false;
}

void Piccolo_Romeo::Mute() {
  Log("YMF288::Mute()\n");
  SetReg(0x007, 0x3f);
  for (uint32_t r = 0x40; r < 0x4f; r++) {
    if (~r & 3) {
      SetReg(0x000 + r, 0x7f);
      SetReg(0x100 + r, 0x7f);
    }
  }
}

void Piccolo_Romeo::SetReg(uint32_t a, uint32_t d) {
  if (avail_) {
    while (IsBusy())
      Sleep(0);
    pcidrv.out8(addr_ + (a < 0x100 ? ADDR0 : ADDR1), a & 0xff);

    while (IsBusy())
      Sleep(0);
    pcidrv.out8(addr_ + (a < 0x100 ? DATA0 : DATA1), d & 0xff);
  }
}
