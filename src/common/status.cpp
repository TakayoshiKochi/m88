#include "common/status.h"

#include <windows.h>
#include <assert.h>

// #define LOGNAME "common_status"
#include "common/diag.h"

void StatusDisplay::FDAccess(uint32_t dr, bool hd, bool active) {
  dr &= 1;
  if (!(litstat_[dr] & 4)) {
    litstat_[dr] = (hd ? 0x22 : 0) | (active ? 0x11 : 0) | 4;
  } else {
    litstat_[dr] = (litstat_[dr] & 0x0f) | (hd ? 0x20 : 0) | (active ? 0x10 : 0) | 9;
  }
}

void StatusDisplay::Show(int priority, int duration, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ShowV(priority, duration, msg, args);
  va_end(args);
}

void StatusDisplay::ShowV(int priority, int duration, const char* msg, va_list args) {
  std::lock_guard<std::mutex> lock(mtx_);

  if (current_priority_ < priority)
    if (!duration || (GetTickCount64() + duration - current_end_time_) < 0)
      return;

  Entry entry{};

  char msgbuf[1024];
  int tl = vsprintf(msgbuf, msg, args);
  assert(tl < 128);

  entry.msg = msgbuf;
  entry.end_time = GetTickCount64() + duration;
  entry.priority = priority;
  entry.clear = duration != 0;
  entries_.emplace_back(std::move(entry));

  Log("reg : [%s] p:%5d d:%8d\n", entry->msg, entry->priority, entry->duration);
  update_message_ = true;
}

void StatusDisplay::Clean() {
  uint64_t c = GetTickCount64();
  for (auto it = entries_.begin(); it < entries_.end();) {
    if (it->end_time < c || !it->clear) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}
