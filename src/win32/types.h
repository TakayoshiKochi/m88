// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  $Id: types.h,v 1.10 1999/12/28 10:34:53 cisc Exp $

#pragma once

#include <windows.h>

#include <stdint.h>

//  固定長型とか
using uint = unsigned int;
using ulong = unsigned long;

// 8 bit 数値をまとめて処理するときに使う型
typedef uint32_t packed;
#define PACK(p) ((p) | ((p) << 8) | ((p) << 16) | ((p) << 24))

// ポインタ値を表現できる整数型
typedef LONG_PTR intpointer;

#undef PTR_IDBIT
#define MEMCALL
