// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.cpp,v 1.3 2000/08/06 09:58:51 cisc Exp $

#include "pc88/cmt.h"

#include <algorithm>

#include "common/io_bus.h"
#include "common/scheduler.h"
#include "common/status_bar.h"
#include "common/time_constants.h"
#include "services/tape_manager.h"

// #define LOGNAME "cmt"
#include "common/diag.h"

namespace pc8801 {

constexpr int kTickHz = 4800;
constexpr int64_t kNanoSecsPerCmtTick = kNanoSecsPerSec / kTickHz;

CMT::CMT() : Device(DEV_ID('C', 'M', 'T', '0')) {}

CMT::~CMT() {
  tape_manager_->Close();
  if (scheduler_)
    SetTimer(0);

  current_pos_ = 0;
  current_offset_ = 0;
}

bool CMT::Init(Scheduler* sched, IOBus* bus, services::TapeManager* tape_manager, int pinput) {
  scheduler_ = sched;
  bus_ = bus;
  tape_manager_ = tape_manager;
  pinput_ = pinput;

  motor_on_ = false;
  timer_count_ = 0;
  timer_remain_ = 0;
  tick_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  即座にデータを要求する
//
void CMT::RequestData(uint32_t, uint32_t) {
  if (mode_ != t88::kData)
    return;
  scheduler_->SetEventNS(event_, kNanoSecsPerTick, this, static_cast<TimeFunc>(&CMT::TimerCallback),
                         0, false);
}

void CMT::TapeOpened(uint32_t, uint32_t) {
  if (!tape_manager_->IsOpen())
    return;
  tags_ = tape_manager_->tags();
  current_pos_ = 0;
  Rewind(true);
}

void CMT::Out30(uint32_t, uint32_t d) {
  // bit3:
  // 0 - motor off
  // 1 - motor on
  Motor(!!(d & 0b00001000));
}

uint32_t CMT::In40(uint32_t) {
  // bit2:
  // 0 - carrier off
  // 1 - carrier on
  return IOBus::Active(Carrier() ? 4 : 0, 4);
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t CMT::GetStatusSize() {
  return sizeof(Status);
}

bool CMT::SaveStatus(uint8_t* s) {
  auto* status = reinterpret_cast<Status*>(s);
  status->rev = ssrev;
  status->motor = motor_on_;
  status->current_pos = GetPos();
  status->current_offset = current_offset_;
  g_status_bar->Show(0, 1000, "tapesave: %d", status->current_pos);
  return true;
}

bool CMT::LoadStatus(const uint8_t* s) {
  // TODO: Restore opened file.
  const auto* status = reinterpret_cast<const Status*>(s);
  if (status->rev != ssrev)
    return false;
  motor_on_ = status->motor;
  Seek(status->current_pos, status->current_offset);
  g_status_bar->Show(0, 1000, "tapesave: %d", GetPos());
  return true;
}

bool CMT::Rewind(bool timer) {
  current_pos_ = 0;
  if (event_) {
    scheduler_->DelEvent(event_);
    event_ = nullptr;
  }

  if (!tags_->empty()) {
    tick_ = 0;

    // バージョン確認
    // 最初のタグはバージョンタグになるはず？
    if (tags_->at(current_pos_).id != t88::kVersion || tags_->at(current_pos_).length < 2 ||
        *(uint16_t*)(tags_->at(current_pos_).data.get()) != t88::kT88Version)
      return false;

    ++current_pos_;
    ProcessTags(timer);
  }
  return true;
}

void CMT::Motor(bool on) {
  if (motor_on_ == on)
    return;

  if (on) {
    g_status_bar->Show(10, 2000, "Motor on: %d %d", timer_remain_, timer_count_);
    time_ = scheduler_->GetTimeNS();
    if (timer_remain_)
      event_ = scheduler_->AddEventNS(timer_count_ * kNanoSecsPerCmtTick, this,
                                      static_cast<TimeFunc>(&CMT::TimerCallback), 1, false);
    motor_on_ = true;
  } else {
    if (timer_count_) {
      int td = (scheduler_->GetTimeNS() - time_) / kNanoSecsPerCmtTick;
      timer_remain_ = std::max(10U, timer_remain_ - td);
      scheduler_->DelEvent(event_);
      event_ = nullptr;
      g_status_bar->Show(10, 2000, "Motor off: %d %d", timer_remain_, timer_count_);
    }
    motor_on_ = false;
  }
}

// ---------------------------------------------------------------------------
//  キャリア確認
//
inline bool CMT::Carrier() const {
  if (mode_ == t88::kMark) {
    Log("*");
    return true;
  }
  return false;
}

void CMT::ProcessTags(const bool timer) {
  while (current_pos_ < tags_->size()) {
    Log("TAG %d\n", current_pos_);
    switch (tags_->at(current_pos_).id) {
      case t88::kEnd:
        mode_ = t88::kBlank;
        current_pos_ = 0;
        timer_count_ = 0;
        g_status_bar->Show(50, 0, "end of tape", tick_);
        return;

      case t88::kBlank:
      case t88::kSpace:
      case t88::kMark: {
        const auto* tag = reinterpret_cast<t88::BlankTag*>(tags_->at(current_pos_).data.get());
        mode_ = static_cast<t88::TagID>(tags_->at(current_pos_).id);

        if (tag->start_tick + tag->length_in_ticks - tick_ <= 0)
          break;

        if (timer)
          SetTimer(tag->start_tick + tag->length_in_ticks - tick_);
        else
          timer_count_ = tag->start_tick + tag->length_in_ticks - tick_;

        ++current_pos_;
        return;
      }

      case t88::kData: {
        auto* tag = reinterpret_cast<t88::DataTag*>(tags_->at(current_pos_).data.get());
        mode_ = t88::kData;

        data_ = tag->data;
        data_size_ = tag->length;
        data_type_ = tag->type;
        current_offset_ = 0;

        if (!data_size_)
          break;

        ++current_pos_;
        if (timer)
          SetTimer(tag->GetTicksPerByte());
        else
          timer_count_ = tag->tick;
        return;
      }

      default:
        break;
    }
    ++current_pos_;
  }
}

void CMT::TimerCallback(uint32_t) {
  tick_ += timer_count_;
  g_status_bar->Show(50, 0, "tape: %d", tick_);

  if (mode_ == t88::kData) {
    Send(*data_);
    ++data_;
    ++current_offset_;
    if (--data_size_ > 0) {
      SetTimer(data_type_ & t88::k1200baud ? t88::kTicksPerByte1200baud
                                           : t88::kTicksPerByte600baud);
      return;
    }
    Log("\n");
  }
  ProcessTags();
}

// ---------------------------------------------------------------------------
//  バイト転送
//
inline void CMT::Send(uint32_t byte) {
  Log("%.2x ", byte);
  bus_->Out(pinput_, byte);
}

// ---------------------------------------------------------------------------
//  タイマー更新
//
void CMT::SetTimer(int ticks) {
  if (ticks > 100)
    Log("TimerCallback: %d\n", ticks);
  scheduler_->DelEvent(event_);
  event_ = nullptr;
  timer_count_ = ticks;
  if (motor_on_) {
    time_ = scheduler_->GetTimeNS();
    if (ticks)
      event_ = scheduler_->AddEventNS(ticks * kNanoSecsPerCmtTick, this,
                                      static_cast<TimeFunc>(&CMT::TimerCallback), 0, false);
  } else {
    timer_remain_ = ticks;
  }
}

uint32_t CMT::GetPos() const {
  if (motor_on_) {
    if (timer_count_)
      return tick_ + (scheduler_->GetTimeNS() - time_) / kNanoSecsPerCmtTick;
    else
      return tick_;
  } else {
    return tick_ + timer_count_ - timer_remain_;
  }
}

bool CMT::Seek(uint32_t newpos, uint32_t off) {
  if (!Rewind(false))
    return false;

  while (current_pos_ < tags_->size() && (tick_ + timer_count_) < newpos) {
    tick_ += timer_count_;
    ProcessTags(false);
  }
  if (current_pos_ >= tags_->size())
    return false;

  switch (tags_->at(current_pos_ - 1).id) {
    case t88::kBlank:
    case t88::kSpace:
    case t88::kMark: {
      mode_ = tags_->at(current_pos_ - 1).id;
      int l = tick_ + timer_count_ - newpos;
      tick_ = newpos;
      SetTimer(l);
    } break;

    case t88::kData:
      mode_ = t88::kData;
      current_offset_ = off;
      // newpos = tick_ + offset_ * (data_type_ ? 44 : 88);
      data_ += current_offset_;
      data_size_ -= current_offset_;
      SetTimer(data_type_ & t88::k1200baud ? t88::kTicksPerByte1200baud
                                           : t88::kTicksPerByte600baud);
      break;

    default:
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor CMT::descriptor = {CMT::indef, CMT::outdef};

const Device::OutFuncPtr CMT::outdef[] = {static_cast<OutFuncPtr>(&CMT::RequestData),
                                          static_cast<OutFuncPtr>(&CMT::TapeOpened),
                                          static_cast<OutFuncPtr>(&CMT::Out30)};

const Device::InFuncPtr CMT::indef[] = {static_cast<InFuncPtr>(&CMT::In40)};
}  // namespace pc8801
