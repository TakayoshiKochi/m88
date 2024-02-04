// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.cpp,v 1.3 2000/08/06 09:58:51 cisc Exp $

#include "services/tape_manager.h"

#include <algorithm>

#include "common/io_bus.h"
#include "common/file.h"
#include "common/status_bar.h"

#define LOGNAME "tape"
#include "common/diag.h"

constexpr char T88ID[] = "PC-8801 Tape Image(T88)";
constexpr int T88_HEADER_SIZE = 24;
constexpr int kSioTickHz = 4800;
constexpr int64_t kNanoSecsPerSioTick = kNanoSecsPerSec / kSioTickHz;

namespace services {
TapeManager::TapeManager() : Device(DEV_ID('T', 'A', 'P', 'E')) {}

TapeManager::~TapeManager() {
  Close();
}

bool TapeManager::Init(Scheduler* s, IOBus* bus, int pinput) {
  scheduler_ = s;
  bus_ = bus;
  pinput_ = pinput;

  motor_ = false;
  timer_count_ = 0;
  timer_remain_ = 0;
  tick_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  T88 を開く
//
bool TapeManager::Open(const std::string_view file) {
  Close();

  FileIO fio;
  if (!fio.Open(file, FileIO::kReadOnly))
    return false;

  // ヘッダ確認
  char buf[T88_HEADER_SIZE];
  fio.Read(buf, T88_HEADER_SIZE);
  if (memcmp(buf, T88ID, T88_HEADER_SIZE))
    return false;

  // タグのリスト構造を展開
  pos_ = 0;
  do {
    TagHdr hdr{};
    if (fio.Read(&hdr, sizeof(TagHdr)) != sizeof(TagHdr)) {
      Close();
      return false;
    }

    tags_.emplace_back(Tag(hdr.id, hdr.length));
    tags_.back().data = std::make_unique<uint8_t[]>(hdr.length);
    fio.Read(tags_.back().data.get(), hdr.length);
  } while (tags_.back().id);

  return Rewind();
}

bool TapeManager::Close() {
  if (scheduler_)
    SetTimer(0);

  tags_.clear();
  return true;
}

bool TapeManager::Rewind(bool timer) {
  pos_ = 0;
  if (event_) {
    scheduler_->DelEvent(event_);
    event_ = nullptr;
  }

  if (!tags_.empty()) {
    tick_ = 0;

    // バージョン確認
    // 最初のタグはバージョンタグになるはず？
    if (tags_[pos_].id != T_VERSION || tags_[pos_].length < 2 ||
        *(uint16_t*)(tags_[pos_].data.get()) != T88VER)
      return false;

    ++pos_;
    Proceed(timer);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  モータ
//
bool TapeManager::Motor(bool on) {
  if (motor_ == on)
    return true;
  if (on) {
    g_status_bar->Show(10, 2000, "Motor on: %d %d", timer_remain_, timer_count_);
    time_ = scheduler_->GetTimeNS();
    if (timer_remain_)
      event_ = scheduler_->AddEventNS(timer_count_ * kNanoSecsPerSioTick, this,
                                      static_cast<TimeFunc>(&TapeManager::Timer), 1, false);
    motor_ = true;
  } else {
    if (timer_count_) {
      int td = (scheduler_->GetTimeNS() - time_) / kNanoSecsPerSioTick;
      timer_remain_ = std::max(10U, timer_remain_ - td);
      scheduler_->DelEvent(event_);
      event_ = nullptr;
      g_status_bar->Show(10, 2000, "Motor off: %d %d", timer_remain_, timer_count_);
    }
    motor_ = false;
  }
  return true;
}

uint32_t TapeManager::GetPos() const {
  if (motor_) {
    if (timer_count_)
      return tick_ + (scheduler_->GetTimeNS() - time_) / kNanoSecsPerSioTick;
    else
      return tick_;
  } else {
    return tick_ + timer_count_ - timer_remain_;
  }
}

// ---------------------------------------------------------------------------
//  タグを処理
//
void TapeManager::Proceed(const bool timer) {
  while (pos_ < tags_.size()) {
    Log("TAG %d\n", pos_);
    switch (tags_[pos_].id) {
      case T_END:
        mode_ = T_BLANK;
        pos_ = 0;
        g_status_bar->Show(50, 0, "end of tape", tick_);
        timer_count_ = 0;
        return;

      case T_BLANK:
      case T_SPACE:
      case T_MARK: {
        const auto* t = reinterpret_cast<BlankTag*>(tags_[pos_].data.get());
        mode_ = static_cast<Mode>(tags_[pos_].id);

        if (t->pos + t->tick - tick_ <= 0)
          break;

        if (timer)
          SetTimer(t->pos + t->tick - tick_);
        else
          timer_count_ = t->pos + t->tick - tick_;

        ++pos_;
        return;
      }

      case T_DATA: {
        auto* t = reinterpret_cast<DataTag*>(tags_[pos_].data.get());
        mode_ = T_DATA;

        data_ = t->data;
        data_size_ = t->length;
        data_type_ = t->type;
        offset_ = 0;

        if (!data_size_)
          break;

        ++pos_;
        if (timer)
          SetTimer(data_type_ & 0x100 ? 44 : 88);
        else
          timer_count_ = t->tick;
        return;
      }

      default:
        break;
    }
    ++pos_;
  }
}

void TapeManager::Timer(uint32_t) {
  tick_ += timer_count_;
  g_status_bar->Show(50, 0, "tape: %d", tick_);

  if (mode_ == T_DATA) {
    Send(*data_);
    ++data_;
    ++offset_;
    if (--data_size_ > 0) {
      SetTimer(data_type_ & 0x100 ? 44 : 88);
      return;
    }
    Log("\n");
  }
  Proceed();
}

// ---------------------------------------------------------------------------
//  キャリア確認
//
bool TapeManager::Carrier() const {
  if (mode_ == T_MARK) {
    Log("*");
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  タイマー更新
//
void TapeManager::SetTimer(int count) {
  if (count > 100)
    Log("Timer: %d\n", count);
  scheduler_->DelEvent(event_);
  event_ = nullptr;
  timer_count_ = count;
  if (motor_) {
    time_ = scheduler_->GetTimeNS();
    if (count)
      event_ = scheduler_->AddEventNS(count * kNanoSecsPerSioTick, this,
                                      static_cast<TimeFunc>(&TapeManager::Timer), 0, false);
  } else {
    timer_remain_ = count;
  }
}

// ---------------------------------------------------------------------------
//  バイト転送
//
inline void TapeManager::Send(uint32_t byte) {
  Log("%.2x ", byte);
  bus_->Out(pinput_, byte);
}

// ---------------------------------------------------------------------------
//  即座にデータを要求する
//
void TapeManager::RequestData(uint32_t, uint32_t) {
  if (mode_ != T_DATA)
    return;
  scheduler_->SetEventNS(event_, kNanoSecsPerTick, this, static_cast<TimeFunc>(&TapeManager::Timer),
                         0, false);
}

bool TapeManager::Seek(uint32_t newpos, uint32_t off) {
  if (!Rewind(false))
    return false;

  while (pos_ < tags_.size() && (tick_ + timer_count_) < newpos) {
    tick_ += timer_count_;
    Proceed(false);
  }
  if (pos_ >= tags_.size())
    return false;

  switch (tags_[pos_ - 1].id) {
    case T_BLANK:
    case T_SPACE:
    case T_MARK: {
      mode_ = static_cast<Mode>(tags_[pos_ - 1].id);
      int l = tick_ + timer_count_ - newpos;
      tick_ = newpos;
      SetTimer(l);
    } break;

    case T_DATA:
      mode_ = T_DATA;
      offset_ = off;
      // newpos = tick_ + offset_ * (data_type_ ? 44 : 88);
      data_ += offset_;
      data_size_ -= offset_;
      SetTimer(data_type_ ? 44 : 88);
      break;

    default:
      return false;
  }
  return true;
}

void TapeManager::Out30(uint32_t, uint32_t d) {
  Motor(!!(d & 8));
}

uint32_t TapeManager::In40(uint32_t) {
  return IOBus::Active(Carrier() ? 4 : 0, 4);
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t TapeManager::GetStatusSize() {
  return sizeof(Status);
}

bool TapeManager::SaveStatus(uint8_t* s) {
  auto* status = reinterpret_cast<Status*>(s);
  status->rev = ssrev;
  status->motor = motor_;
  status->pos = GetPos();
  status->offset = offset_;
  g_status_bar->Show(0, 1000, "tapesave: %d", status->pos);
  return true;
}

bool TapeManager::LoadStatus(const uint8_t* s) {
  const auto* status = reinterpret_cast<const Status*>(s);
  if (status->rev != ssrev)
    return false;
  motor_ = status->motor;
  Seek(status->pos, status->offset);
  g_status_bar->Show(0, 1000, "tapesave: %d", GetPos());
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor TapeManager::descriptor = {TapeManager::indef, TapeManager::outdef};

const Device::OutFuncPtr TapeManager::outdef[] = {
    static_cast<OutFuncPtr>(&TapeManager::RequestData),
    static_cast<OutFuncPtr>(&TapeManager::Out30),
};

const Device::InFuncPtr TapeManager::indef[] = {
    static_cast<InFuncPtr>(&TapeManager::In40),
};
}  // namespace services
