// ---------------------------------------------------------------------------
//  misc.h
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: misc.h,v 1.5 2002/05/31 09:45:20 cisc Exp $

#pragma once

inline int Max(int x, int y) {
  return (x > y) ? x : y;
}
inline int Min(int x, int y) {
  return (x < y) ? x : y;
}
inline int Abs(int x) {
  return x >= 0 ? x : -x;
}

inline int Limit(int v, int max, int min) {
  return v > max ? max : (v < min ? min : v);
}

inline unsigned int BSwap(unsigned int a) {
  return (a >> 24) | ((a >> 8) & 0xff00) | ((a << 8) & 0xff0000) | (a << 24);
}

inline unsigned int NtoBCD(unsigned int a) {
  return ((a / 10) << 4) + (a % 10);
}

inline unsigned int BCDtoN(unsigned int v) {
  return (v >> 4) * 10 + (v & 15);
}

