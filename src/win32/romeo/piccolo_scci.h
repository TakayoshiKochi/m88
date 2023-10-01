#pragma once

#include "win32/romeo/piccolo.h"
#include "win32/romeo/scci.h"

#include <stdint.h>

class Piccolo_SCCI : public Piccolo {
 public:
  Piccolo_SCCI();
  ~Piccolo_SCCI() override;

  int Init() override;
  int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc) override;
  void SetReg(uint32_t addr, uint32_t data) override;

 private:
};
