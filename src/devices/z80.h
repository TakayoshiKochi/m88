// ---------------------------------------------------------------------------
//  Z80 emulator.
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  Z80 のレジスタ定義
// ---------------------------------------------------------------------------
//  $Id: Z80.h,v 1.1.1.1 1999/02/19 09:00:40 cisc Exp $

#ifndef Z80_H
#define Z80_H

#include <stdint.h>

#define Z80_WORDREG_IN_INT

// ---------------------------------------------------------------------------
//  Z80 のレジスタセット
// ---------------------------------------------------------------------------

struct Z80Reg {
#ifdef Z80_WORDREG_IN_INT
#define PAD(p) uint8_t p[sizeof(uint32_t) - sizeof(uint16_t)]
  typedef uint32_t wordreg;
#else
#define PAD(p)
  typedef uint16_t wordreg;
#endif

  union regs {
    struct shorts {
      wordreg af;
      wordreg hl, de, bc, ix, iy, sp;
    } w;
    struct words {
      uint8_t flags, a;
      PAD(p1);
      uint8_t l, h;
      PAD(p2);
      uint8_t e, d;
      PAD(p3);
      uint8_t c, b;
      PAD(p4);
      uint8_t xl, xh;
      PAD(p5);
      uint8_t yl, yh;
      PAD(p6);
      uint8_t spl, sph;
      PAD(p7);
    } b;
  } r;

  wordreg r_af, r_hl, r_de, r_bc; /* 裏レジスタ */
  wordreg pc;
  uint8_t ireg;
  uint8_t rreg, rreg7; /* R(0-6 bit), R(7th bit) */
  uint8_t intmode;     /* 割り込みモード */
  bool iff1, iff2;
};

#undef PAD

#endif /* Z80_H */
