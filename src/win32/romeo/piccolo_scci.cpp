/**
 * PICCOLO for SCCI
 *
 * License : GNU General Public License
 */

#include "win32/romeo/piccolo_scci.h"

#include "win32/romeo/scci.h"
#include "win32/romeo/SCCIDefines.h"

// #define LOGNAME "piccolo"
#include "common/diag.h"

HMODULE g_hScci = nullptr;
SoundInterfaceManager* g_pManager = nullptr;
SoundChip* g_pSoundChip = nullptr;

class SpfmIf : public PiccoloChip {
 public:
  explicit SpfmIf(Piccolo* p) : piccolo_(p) {}
  ~SpfmIf() override {
    Log("YMF288::Reset()\n");
    Mute();
  }

  int Init(uint32_t param) override {
    // SCCI Reset
    g_pManager->reset();
    return true;
  }

  void Reset(bool opna) override {
    // SCCI Reset
    g_pManager->reset();
  }

  bool SetReg(uint32_t at, uint32_t addr, uint32_t data) override {
    // SCCIデータ出力
//    if (g_pSoundChip) {
//      g_pSoundChip->setRegister(addr, data);
//    }
    piccolo_->DrvSetReg(at, addr, data);
    return true;
  }

  void SetChannelMask(uint32_t mask) override{};
  void SetVolume(int ch, int value) override{};

 private:
  void Mute() {
    Log("YMF288::Mute()\n");
    for (uint32_t r = 0x40; r < 0x4f; r++) {
      if (~r & 3) {
        g_pSoundChip->setRegister(0x000 + r, 0x7f);
        g_pSoundChip->setRegister(0x100 + r, 0x7f);
      }
    }
  }

  Piccolo* piccolo_;
  // Piccolo::Driver* drv;
};

static bool LoadDLL() {
  if (g_hScci == nullptr) {
    g_hScci = ::LoadLibrary("scci.dll");
    if (!g_hScci) {
      return false;
    }

    auto getSoundInterfaceManager =
        (SCCIFUNC)(::GetProcAddress(g_hScci, "getSoundInterfaceManager"));

    g_pManager = getSoundInterfaceManager();
    if (g_pManager == nullptr) {
      ::FreeLibrary(g_hScci);
      g_hScci = nullptr;
    }
    // インスタンス初期化
    g_pManager->initializeInstance();
  }
  return g_hScci != nullptr;
}

static void FreeDLL() {
  if (g_pSoundChip) {
    g_pManager->releaseSoundChip(g_pSoundChip);
    g_pSoundChip = nullptr;
  }

  if (g_pManager) {
    g_pManager->releaseInstance();
    g_pManager = nullptr;
  }

  if (g_hScci) {
    ::FreeLibrary(g_hScci);
    g_hScci = nullptr;
  }
}
// ---------------------------------------------------------------------------
//
//
Piccolo_SCCI::Piccolo_SCCI() : Piccolo() {}

Piccolo_SCCI::~Piccolo_SCCI() {
  CleanUp();
  FreeDLL();
}

int Piccolo_SCCI::Init() {
  // DLL 用意
  Log("LoadDLL\n");
  if (!LoadDLL()) {
    return PICCOLOE_DLL_NOT_FOUND;
  }

  bool found_device = false;
  // サウンドチップ取得
  g_pSoundChip = g_pManager->getSoundChip(SC_TYPE_YM2608, SC_CLOCK_7987200);
  if (g_pSoundChip != nullptr) {
    found_device = true;
  } else {
    g_pSoundChip = g_pManager->getSoundChip(SC_TYPE_YM2203, SC_CLOCK_3993600);
    if (g_pSoundChip != nullptr) {
      found_device = true;
    }
  }
  avail_ = 3;

  if (!found_device) {
    return PICCOLOE_ROMEO_NOT_FOUND;
  }

  Piccolo::Init();

  return PICCOLO_SUCCESS;
}

int Piccolo_SCCI::GetChip(PICCOLO_CHIPTYPE _type, PiccoloChip** pc) {
  *pc = nullptr;
  Log("GetChip %d\n", _type);

  if (!avail_) {
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;
  }

  Log(" allocated\n");
  *pc = new SpfmIf(this);

  return PICCOLO_SUCCESS;
}

void Piccolo_SCCI::SetReg(uint32_t addr, uint32_t data) {
  if (avail_) {
    g_pSoundChip->setRegister(addr, data);
  }
}
