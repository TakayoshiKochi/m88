#pragma once

#include "piccolo.h"

#include <stdint.h>

class Piccolo_Scci : public Piccolo {
 public:
  Piccolo_Scci();
  virtual ~Piccolo_Scci();
  int Init();
  int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc);
  void SetReg(uint32_t addr, uint32_t data);

 private:
};
