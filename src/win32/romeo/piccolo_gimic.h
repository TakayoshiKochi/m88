//  $Id: piccolo.h,v 1.2 2002/05/31 09:45:22 cisc Exp $

#pragma once

#include "win32/romeo/piccolo.h"

class Piccolo_Gimic : public Piccolo {
 public:
  Piccolo_Gimic();
  ~Piccolo_Gimic() override;
  int Init() override;
  int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc) override;
  void SetReg(uint32_t addr, uint32_t data) override;

 private:
};
