/**
 * PICCOLO for G.I.M.I.C
 *
 * License : GNU General Public License
 */

#include "win32/romeo/piccolo_gimic.h"

#include "win32/romeo/c86ctl.h"
#include "win32/status_bar_win.h"

// #define LOGNAME "piccolo"
#include "common/diag.h"

HMODULE g_mod = nullptr;
IRealChipBase* g_chipbase = nullptr;

struct GMCDRV {
  IGimic* gimic;
  IRealChip* chip;
  int type;
  bool reserve;
};

static GMCDRV gmcdrv = {0, 0, PICCOLO_INVALID, false};

class GimicIf : public PiccoloChip {
 public:
  explicit GimicIf(Piccolo* p) : pic(p) {}
  ~GimicIf() override {
    Log("YMF288::Reset()\n");
    Mute();
  }
  int Init(uint32_t param) override {
    gmcdrv.chip->reset();
    return true;
  }
  void Reset(bool opna) override {
    pic->DrvReset();
    gmcdrv.chip->reset();
    gmcdrv.gimic->setSSGVolume(opna ? 63 : 68);  // FM/PSG比 50%/55%
  }
  bool SetReg(uint32_t at, uint32_t addr, uint32_t data) override {
    if (gmcdrv.chip) {
      gmcdrv.chip->out(addr, data);
    }
    // pic->DrvSetReg(at, addr, data);
    return true;
  }
  void SetChannelMask(uint32_t mask) override{};
  void SetVolume(int ch, int value) override{};

 private:
  Piccolo* pic;
  //  Piccolo::Driver* drv;
 private:
  void Mute() {
    Log("YMF288::Mute()\n");
    gmcdrv.chip->out(0x07, 0x3f);
    for (uint32_t r = 0x40; r < 0x4f; r++) {
      if (~r & 3) {
        gmcdrv.chip->out(0x000 + r, 0x7f);
        gmcdrv.chip->out(0x100 + r, 0x7f);
      }
    }
  }
};

static bool LoadDLL() {
  if (g_mod == nullptr) {
    g_mod = ::LoadLibrary("c86ctl.dll");
    if (!g_mod) {
      return false;
    }

    typedef HRESULT(WINAPI * TCreateInstance)(REFIID, LPVOID*);
    TCreateInstance pCI;
    pCI = (TCreateInstance)::GetProcAddress(g_mod, "CreateInstance");

    if (pCI) {
      (*pCI)(IID_IRealChipBase, (void**)&g_chipbase);
      if (g_chipbase) {
        g_chipbase->initialize();
      } else {
        ::FreeLibrary(g_mod);
        g_mod = nullptr;
      }
    } else {
      ::FreeLibrary(g_mod);
      g_mod = nullptr;
    }
  }
  return g_mod != nullptr;
}

static void FreeDLL() {
  if (g_chipbase) {
    g_chipbase->deinitialize();
    g_chipbase = 0;
  }
  if (g_mod) {
    ::FreeLibrary(g_mod);
    g_mod = 0;
  }
}

// ---------------------------------------------------------------------------
//
//
Piccolo_Gimic::Piccolo_Gimic() : Piccolo() {}

Piccolo_Gimic::~Piccolo_Gimic() {
  CleanUp();
  FreeDLL();
}

int Piccolo_Gimic::Init() {
  // DLL 用意
  Log("LoadDLL\n");
  if (!LoadDLL()) {
    return PICCOLOE_DLL_NOT_FOUND;
  }

  // OPN3/OPNAの存在確認
  Log("FindDevice\n");
  int devnum = g_chipbase->getNumberOfChip();
  bool found_device = false;
  for (int i = 0; i < devnum; i++) {
    g_chipbase->getChipInterface(i, IID_IGimic, (void**)&gmcdrv.gimic);
    g_chipbase->getChipInterface(i, IID_IRealChip, (void**)&gmcdrv.chip);

    Devinfo info;
    gmcdrv.gimic->getModuleInfo(&info);

    if (strncmp(info.Devname, "GMC-OPNA", sizeof(info.Devname)) == 0) {
      gmcdrv.type = PICCOLO_YM2608;
      found_device = true;
      gmcdrv.chip->reset();
      gmcdrv.gimic->setPLLClock(7987200);
      gmcdrv.gimic->setSSGVolume(63);  // FM/PSG比 50%
      avail_ = 2;
      break;
    }
    if (strncmp(info.Devname, "GMC-OPN3L", sizeof(info.Devname)) == 0) {
      gmcdrv.type = PICCOLO_YMF288;
      found_device = true;
      gmcdrv.chip->reset();
      avail_ = 1;
      break;
    }
  }

  if (!found_device) {
    return PICCOLOE_ROMEO_NOT_FOUND;
  }

  Piccolo::Init();

  return PICCOLO_SUCCESS;
}

int Piccolo_Gimic::GetChip(PICCOLO_CHIPTYPE _type, PiccoloChip** _pc) {
  *_pc = 0;
  Log("GetChip %d\n", _type);

  if (!avail_) {
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;
  }

  if (_type != PICCOLO_YMF288 && _type != PICCOLO_YM2608) {
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;
  }

  Log(" Type: YMF288\n");
  if (gmcdrv.type == PICCOLO_INVALID) {
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;
  }
  if (gmcdrv.reserve) {
    return PICCOLOE_HARDWARE_IN_USE;
  }

  Log(" allocated\n");
  gmcdrv.reserve = true;
  *_pc = new GimicIf(this);

  return PICCOLO_SUCCESS;
}

void Piccolo_Gimic::SetReg(uint32_t addr, uint32_t data) {
  if (avail_) {
    gmcdrv.chip->out(addr, data);
  }
}
