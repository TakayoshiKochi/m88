// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.h,v 1.2 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include <stdint.h>

#include <memory>

namespace t88 {

constexpr char kT88FileHeader[] = "PC-8801 Tape Image(T88)";
constexpr int kT88FileHeaderSize = 24;
constexpr uint16_t kT88Version = 0x100;
constexpr uint16_t k1200baud = 0x100;
constexpr int kTicksPerByte1200baud = 44;
constexpr int kTicksPerByte600baud = 88;

enum TagID : uint16_t {
  kEnd = 0,
  kVersion = 1,
  kBlank = 0x100,
  kData = 0x101,
  kSpace = 0x102,
  kMark = 0x103,
};

struct TagHeader {
  TagID id;
  uint16_t length;
};

struct BlankTag {
  uint32_t start_tick;
  uint32_t length_in_ticks;
};

struct DataTag {
  [[nodiscard]] int GetTicksPerByte() const {
    return type & k1200baud ? kTicksPerByte1200baud : kTicksPerByte600baud;
  }
  uint32_t pos;
  uint32_t tick;
  uint16_t length;
  uint16_t type;
  uint8_t data[1];
};

struct Tag {
  Tag(TagID id, uint16_t length) : id(id), length(length) {}
  TagID id;
  uint16_t length;
  std::unique_ptr<uint8_t[]> data;
};

}  // namespace t88
