/**
 * PICCOLO for SCCI
 *
 * License : GNU General Public License
 */

#include "win32/romeo/piccolo_scci.h"

#include "common/status.h"
#include "win32/romeo/scci.h"
#include "win32/romeo/SCCIDefines.h"

#define LOGNAME "piccolo"
#include "common/diag.h"

HMODULE g_hScci = NULL;
SoundInterfaceManager* g_pManager = NULL;
SoundChip* g_pSoundChip = NULL;

class SpfmIf : public PiccoloChip {
 public:
  SpfmIf(Piccolo* p) : pic(p) {}
  ~SpfmIf() {
    Log("YMF288::Reset()\n");
    Mute();
  }
  int Init(uint32_t param) {
    // SCCI Reset
    g_pManager->reset();
    return true;
  }
  void Reset(bool opna) {
    // SCCI Reset
    g_pManager->reset();
  }
  bool SetReg(uint32_t at, uint32_t addr, uint32_t data) {
    // SCCIデータ出力
    if (g_pSoundChip) {
      g_pSoundChip->setRegister(addr, data);
    }
    return true;
  }
  void SetChannelMask(uint32_t mask){};
  void SetVolume(int ch, int value){};

 private:
  Piccolo* pic;
  //	Piccolo::Driver* drv;
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
};

static bool LoadDLL() {
  if (g_hScci == NULL) {
    g_hScci = ::LoadLibrary("scci.dll");
    if (!g_hScci) {
      return false;
    }

    SCCIFUNC getSoundInterfaceManager =
        (SCCIFUNC)(::GetProcAddress(g_hScci, "getSoundInterfaceManager"));

    g_pManager = getSoundInterfaceManager();
    if (g_pManager == NULL) {
      ::FreeLibrary(g_hScci);
      g_hScci = NULL;
    }
    // インスタンス初期化
    g_pManager->initializeInstance();
  }
  return g_hScci ? true : false;
}

static void FreeDLL() {
  if (g_pSoundChip) {
    g_pManager->releaseSoundChip(g_pSoundChip);
    g_pSoundChip = NULL;
  }

  if (g_pManager) {
    g_pManager->releaseInstance();
    g_pManager = NULL;
  }

  if (g_hScci) {
    ::FreeLibrary(g_hScci);
    g_hScci = NULL;
  }
}
// ---------------------------------------------------------------------------
//
//
Piccolo_Scci::Piccolo_Scci() : Piccolo() {}

Piccolo_Scci::~Piccolo_Scci() {
  CleanUp();
  FreeDLL();
}

int Piccolo_Scci::Init() {
  // DLL 用意
  Log("LoadDLL\n");
  if (!LoadDLL()) {
    return PICCOLOE_DLL_NOT_FOUND;
  }

  bool found_device = false;
  // サウンドチップ取得
  g_pSoundChip = g_pManager->getSoundChip(SC_TYPE_YM2608, SC_CLOCK_7987200);
  if (g_pSoundChip != NULL) {
    found_device = true;
  } else {
    g_pSoundChip = g_pManager->getSoundChip(SC_TYPE_YM2203, SC_CLOCK_3993600);
    if (g_pSoundChip != NULL) {
      found_device = true;
    }
  }
  avail = 3;

  if (found_device == false) {
    return PICCOLOE_ROMEO_NOT_FOUND;
  }

  Piccolo::Init();

  return PICCOLO_SUCCESS;
}

int Piccolo_Scci::GetChip(PICCOLO_CHIPTYPE _type, PiccoloChip** _pc) {
  *_pc = 0;
  Log("GetChip %d\n", _type);

  if (!avail) {
    return PICCOLOE_HARDWARE_NOT_AVAILABLE;
  }

  Log(" allocated\n");
  *_pc = new SpfmIf(this);

  return PICCOLO_SUCCESS;
}

void Piccolo_Scci::SetReg(uint32_t addr, uint32_t data) {
  if (avail) {
    g_pSoundChip->setRegister(addr, data);
  }
}
