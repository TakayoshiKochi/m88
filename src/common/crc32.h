// Copyright (C) 2024 Takayoshi Kochi
// See LICENSE.md for more information.

#pragma once

#include <mutex>

#include <stdint.h>

class CRC32 {
 public:
  // static method only class
  CRC32() = delete;
  ~CRC32() = delete;

  static uint32_t Calc(const uint8_t* data, size_t size);

  static constexpr uint32_t crc32_poly = 0x04c11db7;

 private:
  static void MakeTable();

  static uint32_t table_[256];
  static std::once_flag once_;
};
