// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.cpp,v 1.3 2000/08/06 09:58:51 cisc Exp $

#include "win32/headers.h"

#include <algorithm>

#include "pc88/tapemgr.h"
#include "win32/file.h"
#include "win32/status.h"

#define LOGNAME "tape"
#include "common/diag.h"

#define T88ID "PC-8801 Tape Image(T88)"

// ---------------------------------------------------------------------------
//  構築
//
TapeManager::TapeManager() : Device(DEV_ID('T', 'A', 'P', 'E')), tags(0), scheduler(0), event(0) {
  datasize = 0;
  datatype = 0;
  pinput = 0;
  pos = 0;
  bus = 0;
  offset = 0;
  mode = T_BLANK;
  motor = false;
}

// ---------------------------------------------------------------------------
//  破棄
//
TapeManager::~TapeManager() {
  Close();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool TapeManager::Init(Scheduler* s, IOBus* b, int pi) {
  scheduler = s;
  bus = b;
  pinput = pi;

  motor = false;
  timercount = 0;
  timerremain = 0;
  tick = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  T88 を開く
//
bool TapeManager::Open(const char* file) {
  Close();

  FileIO fio;
  if (!fio.Open(file, FileIO::readonly))
    return false;

  // ヘッダ確認
  char buf[24];
  fio.Read(buf, 24);
  if (memcmp(buf, T88ID, 24))
    return false;

  // タグのリスト構造を展開
  Tag* prv = 0;
  do {
    TagHdr hdr;
    fio.Read(&hdr, 4);

    Tag* tag = (Tag*)new uint8_t[sizeof(Tag) - 1 + hdr.length];
    if (!tag) {
      Close();
      return false;
    }

    tag->prev = prv;
    tag->next = 0;
    (prv ? prv->next : tags) = tag;
    tag->id = hdr.id;
    tag->length = hdr.length;
    fio.Read(tag->data, tag->length);
    prv = tag;
  } while (prv->id);

  if (!Rewind())
    return false;
  return true;
}

// ---------------------------------------------------------------------------
//  とじる
//
bool TapeManager::Close() {
  if (scheduler)
    SetTimer(0);
  while (tags) {
    Tag* n = tags->next;
    delete[] tags;
    tags = n;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  まきもどす
//
bool TapeManager::Rewind(bool timer) {
  pos = tags;
  scheduler->DelEvent(event), event = 0;
  if (pos) {
    tick = 0;

    // バージョン確認
    // 最初のタグはバージョンタグになるはず？
    if (pos->id != T_VERSION || pos->length < 2 || *(uint16_t*)pos->data != T88VER)
      return false;

    pos = pos->next;
    Proceed(timer);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  モータ
//
bool TapeManager::Motor(bool s) {
  if (motor == s)
    return true;
  if (s) {
    statusdisplay.Show(10, 2000, "Motor on: %d %d", timerremain, timercount);
    time = scheduler->GetTime();
    if (timerremain)
      event = scheduler->AddEvent(timercount * 125 / 6, this,
                                  static_cast<TimeFunc>(&TapeManager::Timer));
    motor = true;
  } else {
    if (timercount) {
      int td = (scheduler->GetTime() - time) * 6 / 125;
      timerremain = std::max(10U, timerremain - td);
      scheduler->DelEvent(event), event = 0;
      statusdisplay.Show(10, 2000, "Motor off: %d %d", timerremain, timercount);
    }
    motor = false;
  }
  return true;
}

// ---------------------------------------------------------------------------

uint32_t TapeManager::GetPos() {
  if (motor) {
    if (timercount)
      return tick + (scheduler->GetTime() - time) * 6 / 125;
    else
      return tick;
  } else {
    return tick + timercount - timerremain;
  }
}

// ---------------------------------------------------------------------------
//  タグを処理
//
void TapeManager::Proceed(bool timer) {
  while (pos) {
    Log("TAG %d\n", pos->id);
    switch (pos->id) {
      case T_END:
        mode = T_BLANK;
        pos = 0;
        statusdisplay.Show(50, 0, "end of tape", tick);
        timercount = 0;
        return;

      case T_BLANK:
      case T_SPACE:
      case T_MARK: {
        BlankTag* t = (BlankTag*)pos->data;
        mode = (Mode)pos->id;

        if (t->pos + t->tick - tick <= 0)
          break;

        if (timer)
          SetTimer(t->pos + t->tick - tick);
        else
          timercount = t->pos + t->tick - tick;

        pos = pos->next;
        return;
      }

      case T_DATA: {
        DataTag* t = (DataTag*)pos->data;
        mode = T_DATA;

        data = t->data;
        datasize = t->length;
        datatype = t->type;
        offset = 0;

        if (!datasize)
          break;

        pos = pos->next;
        if (timer)
          SetTimer(datatype & 0x100 ? 44 : 88);
        else
          timercount = t->tick;
        return;
      }
    }
    pos = pos->next;
  }
}

// ---------------------------------------------------------------------------
//
//
void IOCALL TapeManager::Timer(uint32_t) {
  tick += timercount;
  statusdisplay.Show(50, 0, "tape: %d", tick);

  if (mode == T_DATA) {
    Send(*data++);
    offset++;
    if (--datasize > 0) {
      SetTimer(datatype & 0x100 ? 44 : 88);
      return;
    }
    Log("\n");
  }
  Proceed();
}

// ---------------------------------------------------------------------------
//  キャリア確認
//
bool TapeManager::Carrier() {
  if (mode == T_MARK) {
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
  scheduler->DelEvent(event), event = 0;
  timercount = count;
  if (motor) {
    time = scheduler->GetTime();
    if (count)  // 100000/4800
      event =
          scheduler->AddEvent(count * 125 / 6, this, static_cast<TimeFunc>(&TapeManager::Timer));
  } else
    timerremain = count;
}

// ---------------------------------------------------------------------------
//  バイト転送
//
inline void TapeManager::Send(uint32_t byte) {
  Log("%.2x ", byte);
  bus->Out(pinput, byte);
}

// ---------------------------------------------------------------------------
//  即座にデータを要求する
//
void TapeManager::RequestData(uint32_t, uint32_t) {
  if (mode == T_DATA) {
    scheduler->SetEvent(event, 1, this, static_cast<TimeFunc>(&TapeManager::Timer));
  }
}

// ---------------------------------------------------------------------------
//  シークする
//
bool TapeManager::Seek(uint32_t newpos, uint32_t off) {
  if (!Rewind(false))
    return false;

  while (pos && (tick + timercount) < newpos) {
    tick += timercount;
    Proceed(false);
  }
  if (!pos)
    return false;

  switch (pos->prev->id) {
    int l;

    case T_BLANK:
    case T_SPACE:
    case T_MARK:
      mode = (Mode)pos->prev->id;
      l = tick + timercount - newpos;
      tick = newpos;
      SetTimer(l);
      break;

    case T_DATA:
      mode = T_DATA;
      offset = off;
      newpos = tick + offset * (datatype ? 44 : 88);
      data += offset;
      datasize -= offset;
      SetTimer(datatype ? 44 : 88);
      break;

    default:
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------

void IOCALL TapeManager::Out30(uint32_t, uint32_t d) {
  Motor(!!(d & 8));
}

uint32_t IOCALL TapeManager::In40(uint32_t) {
  return IOBus::Active(Carrier() ? 4 : 0, 4);
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL TapeManager::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL TapeManager::SaveStatus(uint8_t* s) {
  Status* status = (Status*)s;
  status->rev = ssrev;
  status->motor = motor;
  status->pos = GetPos();
  status->offset = offset;
  statusdisplay.Show(0, 1000, "tapesave: %d", status->pos);
  return true;
}

bool IFCALL TapeManager::LoadStatus(const uint8_t* s) {
  const Status* status = (const Status*)s;
  if (status->rev != ssrev)
    return false;
  motor = status->motor;
  Seek(status->pos, status->offset);
  statusdisplay.Show(0, 1000, "tapesave: %d", GetPos());
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor TapeManager::descriptor = {TapeManager::indef, TapeManager::outdef};

const Device::OutFuncPtr TapeManager::outdef[] = {
    static_cast<Device::OutFuncPtr>(&RequestData),
    static_cast<Device::OutFuncPtr>(&Out30),
};

const Device::InFuncPtr TapeManager::indef[] = {
    static_cast<Device::InFuncPtr>(&In40),
};
