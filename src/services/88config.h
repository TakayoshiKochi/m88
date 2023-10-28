// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: 88config.h,v 1.9 2000/08/06 09:59:20 cisc Exp $

#pragma once

#include "if/ifcommon.h"
#include "pc88/config.h"

#include <string_view>

namespace services {
void SaveConfig(pc8801::Config* cfg, const std::string_view inifile, bool writedefault);
void LoadConfig(pc8801::Config* cfg, const std::string_view inifile, bool applydefault);
void LoadConfigDirectory(pc8801::Config* cfg,
                         const std::string_view inifile,
                         const char* entry,
                         bool readalways);
}  // namespace services
