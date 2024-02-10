// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.h,v 1.2 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include "common/scheduler.h"
#include "common/tape.h"

#include <string_view>
#include <vector>

class IOBus;

namespace services {
class TapeManager {
 public:
  TapeManager() = default;
  ~TapeManager();

  bool Init(IOBus* bus, uint32_t popen);

  bool Open(std::string_view file);
  void Close();

  [[nodiscard]] bool IsOpen() const { return !tags_.empty(); }
  std::vector<t88::Tag>* tags() { return &tags_; }

 private:
  std::vector<t88::Tag> tags_;
  IOBus* bus_ = nullptr;
  uint32_t popen_ = 0;
};
}  // namespace services
