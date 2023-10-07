// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  デバイスと進行管理
// ---------------------------------------------------------------------------
//  $Id: pc88.cpp,v 1.53 2003/09/28 14:35:35 cisc Exp $

//  Memory Bus Banksize <= 0x400

#include "pc88/pc88.h"

#include <algorithm>
#include <memory>

#include "common/status.h"

#include "pc88/base.h"
#include "pc88/beep.h"
#include "pc88/calendar.h"
#include "pc88/config.h"
#include "pc88/crtc.h"
#include "pc88/fdc.h"
#include "pc88/intc.h"
#include "pc88/joypad.h"
#include "pc88/kanjirom.h"
#include "pc88/memory.h"
#include "pc88/opnif.h"
#include "pc88/pd8257.h"
#include "pc88/screen.h"
#include "pc88/sio.h"
#include "pc88/subsys.h"
#include "services/diskmgr.h"
#include "services/tapemgr.h"
#include "win32/monitor/loadmon.h"

// #define LOGNAME "pc88"
#include "common/diag.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築・破棄
//
PC88::PC88()
    : scheduler_(this),
      cpu1(DEV_ID('C', 'P', 'U', '1')),
      cpu2(DEV_ID('C', 'P', 'U', '2')),
      scrn(0),
      intc(0),
      fdc(0),
      siotape(0),
      caln(0),
      diskmgr(0),
      siomidi(0),
      joypad(0) {
  assert((1 << MemoryManager::pagebits) <= 0x400);
  DIAGINIT(&cpu1);
}

PC88::~PC88() {
  //  devlist.CleanUp();
  delete scrn;
  delete intc;
  delete fdc;
  delete siotape;
  delete siomidi;
  delete caln;
  delete joypad;
}

// ---------------------------------------------------------------------------
//  初期化
//
bool PC88::Init(Draw* draw, DiskManager* disk, TapeManager* tape) {
  draw_ = draw;
  diskmgr = disk;
  tapemgr = tape;

  if (!scheduler_.Init())
    return false;

  if (!draw_->Init(640, 400, 8))
    return false;

  if (!tapemgr->Init(&scheduler_, nullptr, 0))
    return false;

  MemoryPage *read, *write;

  cpu1.GetPages(&read, &write);
  if (!mm1.Init(0x10000, read, write))
    return false;

  cpu2.GetPages(&read, &write);
  if (!mm2.Init(0x10000, read, write))
    return false;

  if (!bus1.Init(kPortEnd, &devlist) || !bus2.Init(kPortEnd2, &devlist))
    return false;

  if (!ConnectDevices() || !ConnectDevices2())
    return false;

  Reset();
  region.Reset();
  scheduler_.set_clocks_per_tick(1);
  return true;
}

void PC88::DeInit() {
  if (opn1_)
    opn1_->CleanUp();
  if (opn2_)
    opn2_->CleanUp();
  if (beep_)
    beep_->CleanUp();
}

// ---------------------------------------------------------------------------
//  執行
//  1 tick = 10μs
//
// int PC88::Proceed(uint32_t ticks, uint32_t clock, uint32_t ecl) {
//  return int(ProceedNS(ticks * 10000ULL, clock, ecl) / 10000);
//}

int64_t PC88::ProceedNS(int64_t ns, uint32_t clock, uint32_t ecl) {
  scheduler_.set_clocks_per_tick(std::max(1U, clock));
  effective_clocks_per_tick_ = std::max(1U, ecl);
  return scheduler_.ProceedNS(ns);
}

// ---------------------------------------------------------------------------
//  実行
//
int64_t SchedulerImpl::ExecuteNS(int64_t ns) {
  int64_t ns_per_clock = 10000LL / clocks_per_tick_;
  int clocks = int(ns / ns_per_clock);
  // TODO: fix this logic.
  if (clocks == 0)
    clocks = 1;
  return ex_->Execute(clocks) * ns_per_clock;
}

int PC88::Execute(int clocks) {
  LOADBEGIN("Core.CPU");
  int ex = 0;
  if (!(cpumode & stopwhenidle) || subsys_->IsBusy() || fdc->IsBusy()) {
    if ((cpumode & 1) == ms11)
      ex = Z80::ExecDual(&cpu1, &cpu2, clocks);
    else
      ex = Z80::ExecDual2(&cpu1, &cpu2, clocks);
  } else {
    ex = Z80::ExecSingle(&cpu1, &cpu2, clocks);
  }
  return ex;
  LOADEND("Core.CPU");
}

// ---------------------------------------------------------------------------
//  実行クロック数変更
//
void SchedulerImpl::ShortenNS(int64_t ns) {
  // int64 nanos_per_clock = 10000LL / clocks_per_tick_;
  Z80::StopDual(int(ns / (10000LL / clocks_per_tick_)));
}

int64_t SchedulerImpl::GetNS() {
  return Z80::GetCCount() * (10000LL / clocks_per_tick_);
}

// ---------------------------------------------------------------------------
//  VSync
//
void PC88::VSync() {
  g_status_display->UpdateDisplay();
  if (cfgflags & Config::kWatchRegister)
    g_status_display->Show(10, 0, "%.4X(%.2X)/%.4X", cpu1.GetPC(), cpu1.GetReg().ireg,
                           cpu2.GetPC());
}

// ---------------------------------------------------------------------------
//  画面更新
//
void PC88::UpdateScreen(bool refresh) {
  uint32_t dstat = draw_->GetStatus();
  if (dstat & static_cast<uint32_t>(Draw::Status::kShouldRefresh))
    refresh = true;

  LOADBEGIN("Screen");

  if (!updated || refresh) {
    if (!(cfgflags & Config::kDrawPriorityLow) ||
        (dstat & (static_cast<uint32_t>(Draw::Status::kReadyToDraw) |
                  static_cast<uint32_t>(Draw::Status::kShouldRefresh))))
    //      if (dstat & (Draw::readytodraw | Draw::shouldrefresh))
    {
      int bpl;
      uint8_t* image;

      //          crtc->SetSize();
      if (draw_->Lock(&image, &bpl)) {
        Log("(%d -> %d) ", region.top, region.bottom);
        crtc_->UpdateScreen(image, bpl, region, refresh);
        Log("(%d -> %d) ", region.top, region.bottom);
        scrn->UpdateScreen(image, bpl, region, refresh);
        Log("(%d -> %d)\n", region.top, region.bottom);

        bool palchanged = scrn->UpdatePalette(draw_);
        draw_->Unlock();
        updated = palchanged || region.Valid();
      }
    }
  }
  LOADEND("Screen");
  if (draw_->GetStatus() & static_cast<uint32_t>(Draw::Status::kReadyToDraw)) {
    if (updated) {
      updated = false;
      draw_->DrawScreen(region);
      region.Reset();
    } else {
      Draw::Region r;
      r.Reset();
      draw_->DrawScreen(r);
    }
  }
}

// ---------------------------------------------------------------------------
//  リセット
//
void PC88::Reset() {
  bool cd = false;
  if (IsCDSupported())
    cd = (static_cast<uint32_t>(base_->GetBasicMode()) & 0x40) != 0;

  base_->SetFDBoot(cd || diskmgr->GetCurrentDisk(0) >= 0);
  base_->Reset();  // Switch 関係の更新

  bool isv2 = (bus1.In(0x31) & 0x40) != 0;
  bool isn80v2 = (base_->GetBasicMode() == BasicMode::kN80V2);

  if (isv2)
    dmac_->ConnectRd(mem1_->GetTVRAM(), 0xf000, 0x1000);
  else
    dmac_->ConnectRd(mem1_->GetRAM(), 0, 0x10000);
  dmac_->ConnectWr(mem1_->GetRAM(), 0, 0x10000);

  opn1_->SetOPNMode((cfgflags & Config::kEnableOPNA) != 0);
  opn1_->Enable(isv2 || !(cfgflag2 & Config::kDisableOPN44));
  opn2_->SetOPNMode((cfgflags & Config::kOPNAonA8) != 0);
  opn2_->Enable((cfgflags & (Config::kOPNAonA8 | Config::kOPNonA8)) != 0);

  if (!isn80v2)
    opn1_->SetIMask(0x32, 0x80);
  else
    opn1_->SetIMask(0x33, 0x02);

  bus1.Out(kPReset, static_cast<uint32_t>(base_->GetBasicMode()));
  bus1.Out(0x30, 1);
  bus1.Out(0x30, 0);
  bus1.Out(0x31, 0);
  bus1.Out(0x32, 0x80);
  bus1.Out(0x33, isn80v2 ? 0x82 : 0x02);
  bus1.Out(0x34, 0);
  bus1.Out(0x35, 0);
  bus1.Out(0x40, 0);
  bus1.Out(0x53, 0);
  bus1.Out(0x5f, 0);
  bus1.Out(0x70, 0);
  bus1.Out(0x99, cd ? 0x10 : 0x00);
  bus1.Out(0xe2, 0);
  bus1.Out(0xe3, 0);
  bus1.Out(0xe6, 0);
  bus1.Out(0xf1, 1);
  bus2.Out(kPReset2, 0);

  //  g_status_display->Show(10, 1000, "CPUMode = %d", cpumode);
}

// ---------------------------------------------------------------------------
//  デバイス接続
//
bool PC88::ConnectDevices() {
  static const IOBus::Connector c_cpu1[] = {
      {kPReset, IOBus::portout, Z80::reset}, {kPIRQ, IOBus::portout, Z80::irq}, {0, 0, 0}};
  if (!bus1.Connect(&cpu1, c_cpu1))
    return false;
  if (!cpu1.Init(&mm1, &bus1, kPIAck))
    return false;

  static const IOBus::Connector c_base[] = {{kPReset, IOBus::portout, Base::reset},
                                            {kVrtc, IOBus::portout, Base::vrtc},
                                            {0x30, IOBus::portin, Base::in30},
                                            {0x31, IOBus::portin, Base::in31},
                                            {0x40, IOBus::portin, Base::in40},
                                            {0x6e, IOBus::portin, Base::in6e},
                                            {0, 0, 0}};
  base_ = std::make_unique<PC8801::Base>(DEV_ID('B', 'A', 'S', 'E'));
  if (!base_ || !bus1.Connect(base_.get(), c_base))
    return false;
  if (!base_->Init(this))
    return false;
  devlist.Add(tapemgr);

  static const IOBus::Connector c_dmac[] = {
      {kPReset, IOBus::portout, PD8257::kReset}, {0x60, IOBus::portout, PD8257::kSetAddr},
      {0x61, IOBus::portout, PD8257::kSetCount}, {0x62, IOBus::portout, PD8257::kSetAddr},
      {0x63, IOBus::portout, PD8257::kSetCount}, {0x64, IOBus::portout, PD8257::kSetAddr},
      {0x65, IOBus::portout, PD8257::kSetCount}, {0x66, IOBus::portout, PD8257::kSetAddr},
      {0x67, IOBus::portout, PD8257::kSetCount}, {0x68, IOBus::portout, PD8257::kSetMode},
      {0x60, IOBus::portin, PD8257::kGetAddr},   {0x61, IOBus::portin, PD8257::kGetCount},
      {0x62, IOBus::portin, PD8257::kGetAddr},   {0x63, IOBus::portin, PD8257::kGetCount},
      {0x64, IOBus::portin, PD8257::kGetAddr},   {0x65, IOBus::portin, PD8257::kGetCount},
      {0x66, IOBus::portin, PD8257::kGetAddr},   {0x67, IOBus::portin, PD8257::kGetCount},
      {0x68, IOBus::portin, PD8257::kGetStat},   {0, 0, 0}};
  dmac_ = std::make_unique<PC8801::PD8257>(DEV_ID('D', 'M', 'A', 'C'));
  if (!bus1.Connect(dmac_.get(), c_dmac))
    return false;

  static const IOBus::Connector c_crtc[] = {
      {kPReset, IOBus::portout, CRTC::kReset},    {0x50, IOBus::portout, CRTC::kOut},
      {0x51, IOBus::portout, CRTC::kOut},         {0x50, IOBus::portin, CRTC::kGetStatus},
      {0x51, IOBus::portin, CRTC::kIn},           {0x00, IOBus::portout, CRTC::kPCGOut},
      {0x01, IOBus::portout, CRTC::kPCGOut},      {0x02, IOBus::portout, CRTC::kPCGOut},
      {0x33, IOBus::portout, CRTC::kSetKanaMode}, {0, 0, 0}};
  crtc_ = std::make_unique<PC8801::CRTC>(DEV_ID('C', 'R', 'T', 'C'));
  if (!crtc_ || !bus1.Connect(crtc_.get(), c_crtc))
    return false;

  static const IOBus::Connector c_mem1[] = {{kPReset, IOBus::portout, Memory::reset},
                                            {0x31, IOBus::portout, Memory::out31},
                                            {0x32, IOBus::portout, Memory::out32},
                                            {0x33, IOBus::portout, Memory::out33},
                                            {0x34, IOBus::portout, Memory::out34},
                                            {0x35, IOBus::portout, Memory::out35},
                                            {0x5c, IOBus::portout, Memory::out5x},
                                            {0x5d, IOBus::portout, Memory::out5x},
                                            {0x5e, IOBus::portout, Memory::out5x},
                                            {0x5f, IOBus::portout, Memory::out5x},
                                            {0x70, IOBus::portout, Memory::out70},
                                            {0x71, IOBus::portout, Memory::out71},
                                            {0x78, IOBus::portout, Memory::out78},
                                            {0x99, IOBus::portout, Memory::out99},
                                            {0xe2, IOBus::portout, Memory::oute2},
                                            {0xe3, IOBus::portout, Memory::oute3},
                                            {0xf0, IOBus::portout, Memory::outf0},
                                            {0xf1, IOBus::portout, Memory::outf1},
                                            {kVrtc, IOBus::portout, Memory::vrtc},
                                            {0x32, IOBus::portin, Memory::in32},
                                            {0x33, IOBus::portin, Memory::in33},
                                            {0x5c, IOBus::portin, Memory::in5c},
                                            {0x70, IOBus::portin, Memory::in70},
                                            {0x71, IOBus::portin, Memory::in71},
                                            {0xe2, IOBus::portin, Memory::ine2},
                                            {0xe3, IOBus::portin, Memory::ine3},
                                            {0, 0, 0}};
  mem1_ = std::make_unique<PC8801::Memory>(DEV_ID('M', 'E', 'M', '1'));
  if (!mem1_ || !bus1.Connect(mem1_.get(), c_mem1))
    return false;
  if (!mem1_->Init(&mm1, &bus1, crtc_.get(), cpu1.GetWaits()))
    return false;

  // TODO: CRTC is dependent on DMAC's object lifetime. (do not pass unique_ptr here)
  if (!crtc_->Init(&bus1, &scheduler_, dmac_.get()))
    return false;

  static const IOBus::Connector c_knj1[] = {{0xe8, IOBus::portout, KanjiROM::setl},
                                            {0xe9, IOBus::portout, KanjiROM::seth},
                                            {0xe8, IOBus::portin, KanjiROM::readl},
                                            {0xe9, IOBus::portin, KanjiROM::readh},
                                            {0, 0, 0}};
  knj1_ = std::make_unique<PC8801::KanjiROM>(DEV_ID('K', 'N', 'J', '1'));
  if (!knj1_ || !bus1.Connect(knj1_.get(), c_knj1))
    return false;
  if (!knj1_->Init("kanji1.rom"))
    return false;

  static const IOBus::Connector c_knj2[] = {{0xec, IOBus::portout, KanjiROM::setl},
                                            {0xed, IOBus::portout, KanjiROM::seth},
                                            {0xec, IOBus::portin, KanjiROM::readl},
                                            {0xed, IOBus::portin, KanjiROM::readh},
                                            {0, 0, 0}};
  knj2_ = std::make_unique<PC8801::KanjiROM>(DEV_ID('K', 'N', 'J', '2'));
  if (!knj2_ || !bus1.Connect(knj2_.get(), c_knj2))
    return false;
  if (!knj2_->Init("kanji2.rom"))
    return false;

  static const IOBus::Connector c_scrn[] = {
      {kPReset, IOBus::portout, Screen::reset},  {0x30, IOBus::portout, Screen::out30},
      {0x31, IOBus::portout, Screen::out31},     {0x32, IOBus::portout, Screen::out32},
      {0x33, IOBus::portout, Screen::out33},     {0x52, IOBus::portout, Screen::out52},
      {0x53, IOBus::portout, Screen::out53},     {0x54, IOBus::portout, Screen::out54},
      {0x55, IOBus::portout, Screen::out55to5b}, {0x56, IOBus::portout, Screen::out55to5b},
      {0x57, IOBus::portout, Screen::out55to5b}, {0x58, IOBus::portout, Screen::out55to5b},
      {0x59, IOBus::portout, Screen::out55to5b}, {0x5a, IOBus::portout, Screen::out55to5b},
      {0x5b, IOBus::portout, Screen::out55to5b}, {0, 0, 0}};
  scrn = new PC8801::Screen(DEV_ID('S', 'C', 'R', 'N'));
  if (!scrn || !bus1.Connect(scrn, c_scrn))
    return false;
  if (!scrn->Init(&bus1, mem1_.get(), crtc_.get()))
    return false;

  static const IOBus::Connector c_intc[] = {{kPReset, IOBus::portout, INTC::reset},
                                            {kPint0, IOBus::portout, INTC::request},
                                            {kPint1, IOBus::portout, INTC::request},
                                            {kPint2, IOBus::portout, INTC::request},
                                            {kPint3, IOBus::portout, INTC::request},
                                            {kPint4, IOBus::portout, INTC::request},
                                            {kPint5, IOBus::portout, INTC::request},
                                            {kPint6, IOBus::portout, INTC::request},
                                            {kPint7, IOBus::portout, INTC::request},
                                            {0xe4, IOBus::portout, INTC::setreg},
                                            {0xe6, IOBus::portout, INTC::setmask},
                                            {kPIAck, IOBus::portin, INTC::intack},
                                            {0, 0, 0}};
  intc = new PC8801::INTC(DEV_ID('I', 'N', 'T', 'C'));
  if (!intc || !bus1.Connect(intc, c_intc))
    return false;
  if (!intc->Init(&bus1, kPIRQ, kPint0))
    return false;

  static const IOBus::Connector c_subsys[] = {
      {kPReset, IOBus::portout, SubSystem::reset},
      {0xfc, IOBus::portout | IOBus::sync, SubSystem::m_set0},
      {0xfd, IOBus::portout | IOBus::sync, SubSystem::m_set1},
      {0xfe, IOBus::portout | IOBus::sync, SubSystem::m_set2},
      {0xff, IOBus::portout | IOBus::sync, SubSystem::m_setcw},
      {0xfc, IOBus::portin | IOBus::sync, SubSystem::m_read0},
      {0xfd, IOBus::portin | IOBus::sync, SubSystem::m_read1},
      {0xfe, IOBus::portin | IOBus::sync, SubSystem::m_read2},
      {0, 0, 0}};
  subsys_ = std::make_unique<PC8801::SubSystem>(DEV_ID('S', 'U', 'B', ' '));
  if (!subsys_ || !bus1.Connect(subsys_.get(), c_subsys))
    return false;

  static const IOBus::Connector c_sio[] = {{kPReset, IOBus::portout, SIO::reset},
                                           {0x20, IOBus::portout, SIO::setdata},
                                           {0x21, IOBus::portout, SIO::setcontrol},
                                           {kPSIOin, IOBus::portout, SIO::acceptdata},
                                           {0x20, IOBus::portin, SIO::getdata},
                                           {0x21, IOBus::portin, SIO::getstatus},
                                           {0, 0, 0}};
  siotape = new PC8801::SIO(DEV_ID('S', 'I', 'O', ' '));
  if (!siotape || !bus1.Connect(siotape, c_sio))
    return false;
  if (!siotape->Init(&bus1, kPint0, kPSIOReq))
    return false;

  static const IOBus::Connector c_tape[] = {{kPSIOReq, IOBus::portout, TapeManager::requestdata},
                                            {0x30, IOBus::portout, TapeManager::out30},
                                            {0x40, IOBus::portin, TapeManager::in40},
                                            {0, 0, 0}};
  if (!bus1.Connect(tapemgr, c_tape))
    return false;
  if (!tapemgr->Init(&scheduler_, &bus1, kPSIOin))
    return false;

  static const IOBus::Connector c_opn1[] = {
      {kPReset, IOBus::portout, OPNIF::reset},   {0x32, IOBus::portout, OPNIF::setintrmask},
      {0x44, IOBus::portout, OPNIF::setindex0},  {0x45, IOBus::portout, OPNIF::writedata0},
      {0x46, IOBus::portout, OPNIF::setindex1},  {0x47, IOBus::portout, OPNIF::writedata1},
      {kPTimeSync, IOBus::portout, OPNIF::sync}, {0x44, IOBus::portin, OPNIF::readstatus},
      {0x45, IOBus::portin, OPNIF::readdata0},   {0x46, IOBus::portin, OPNIF::readstatusex},
      {0x47, IOBus::portin, OPNIF::readdata1},   {0, 0, 0}};
  opn1_ = std::make_unique<PC8801::OPNIF>(DEV_ID('O', 'P', 'N', '1'));
  if (!opn1_ || !opn1_->Init(&bus1, kPint4, kPOPNio1, &scheduler_))
    return false;
  if (!bus1.Connect(opn1_.get(), c_opn1))
    return false;
  opn1_->SetIMask(0x32, 0x80);

  static const IOBus::Connector c_opn2[] = {{kPReset, IOBus::portout, OPNIF::reset},
                                            {0xaa, IOBus::portout, OPNIF::setintrmask},
                                            {0xa8, IOBus::portout, OPNIF::setindex0},
                                            {0xa9, IOBus::portout, OPNIF::writedata0},
                                            {0xac, IOBus::portout, OPNIF::setindex1},
                                            {0xad, IOBus::portout, OPNIF::writedata1},
                                            {0xa8, IOBus::portin, OPNIF::readstatus},
                                            {0xa9, IOBus::portin, OPNIF::readdata0},
                                            {0xac, IOBus::portin, OPNIF::readstatusex},
                                            {0xad, IOBus::portin, OPNIF::readdata1},
                                            {0, 0, 0}};
  opn2_ = std::make_unique<PC8801::OPNIF>(DEV_ID('O', 'P', 'N', '2'));
  if (!opn2_->Init(&bus1, kPint4, kPOPNio1, &scheduler_))
    return false;
  if (!opn2_ || !bus1.Connect(opn2_.get(), c_opn2))
    return false;
  opn2_->SetIMask(0xaa, 0x80);

  static const IOBus::Connector c_caln[] = {{kPReset, IOBus::portout, Calendar::kReset},
                                            {0x10, IOBus::portout, Calendar::kOut10},
                                            {0x40, IOBus::portout, Calendar::kOut40},
                                            {0x40, IOBus::portin, Calendar::kIn40},
                                            {0, 0, 0}};
  caln = new PC8801::Calendar(DEV_ID('C', 'A', 'L', 'N'));
  if (!caln || !caln->Init())
    return false;
  if (!bus1.Connect(caln, c_caln))
    return false;

  static const IOBus::Connector c_beep[] = {{0x40, IOBus::portout, Beep::out40}, {0, 0, 0}};
  beep_ = std::make_unique<PC8801::Beep>(DEV_ID('B', 'E', 'E', 'P'));
  if (!beep_ || !beep_->Init())
    return false;
  if (!bus1.Connect(beep_.get(), c_beep))
    return false;

  static const IOBus::Connector c_siom[] = {{kPReset, IOBus::portout, SIO::reset},
                                            {0xc2, IOBus::portout, SIO::setdata},
                                            {0xc3, IOBus::portout, SIO::setcontrol},
                                            {0, IOBus::portout, SIO::acceptdata},
                                            {0xc2, IOBus::portin, SIO::getdata},
                                            {0xc3, IOBus::portin, SIO::getstatus},
                                            {0, 0, 0}};
  siomidi = new PC8801::SIO(DEV_ID('S', 'I', 'O', 'M'));
  if (!siomidi || !bus1.Connect(siomidi, c_siom))
    return false;
  if (!siomidi->Init(&bus1, 0, kPSIOReq))
    return false;

  static const IOBus::Connector c_joy[] = {{kPOPNio1, IOBus::portin, JoyPad::getdir},
                                           {kPOPNio2, IOBus::portin, JoyPad::getbutton},
                                           {kVrtc, IOBus::portout, JoyPad::vsync},
                                           {0, 0, 0}};
  joypad = new PC8801::JoyPad();  // DEV_ID('J', 'O', 'Y', ' '));
  if (!joypad)
    return false;
  if (!bus1.Connect(joypad, c_joy))
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  デバイス接続(サブCPU)
//
bool PC88::ConnectDevices2() {
  static const IOBus::Connector c_cpu2[] = {
      {kPReset2, IOBus::portout, Z80::reset}, {kPIRQ2, IOBus::portout, Z80::irq}, {0, 0, 0}};
  if (!bus2.Connect(&cpu2, c_cpu2))
    return false;
  if (!cpu2.Init(&mm2, &bus2, kPIAck2))
    return false;

  static const IOBus::Connector c_mem2[] = {
      {kPIAck2, IOBus::portin, SubSystem::intack},
      {0xfc, IOBus::portout | IOBus::sync, SubSystem::s_set0},
      {0xfd, IOBus::portout | IOBus::sync, SubSystem::s_set1},
      {0xfe, IOBus::portout | IOBus::sync, SubSystem::s_set2},
      {0xff, IOBus::portout | IOBus::sync, SubSystem::s_setcw},
      {0xfc, IOBus::portin | IOBus::sync, SubSystem::s_read0},
      {0xfd, IOBus::portin | IOBus::sync, SubSystem::s_read1},
      {0xfe, IOBus::portin | IOBus::sync, SubSystem::s_read2},
      {0, 0, 0}};
  if (!subsys_ || !bus2.Connect(subsys_.get(), c_mem2))
    return false;
  if (!subsys_->Init(&mm2))
    return false;

  static const IOBus::Connector c_fdc[] = {
      {kPReset2, IOBus::portout, FDC::reset},    {0xfb, IOBus::portout, FDC::setdata},
      {0xf4, IOBus::portout, FDC::drivecontrol}, {0xf8, IOBus::portout, FDC::motorcontrol},
      {0xf8, IOBus::portin, FDC::tcin},          {0xfa, IOBus::portin, FDC::getstatus},
      {0xfb, IOBus::portin, FDC::getdata},       {0, 0, 0}};
  fdc = new PC8801::FDC(DEV_ID('F', 'D', 'C', ' '));
  if (!bus2.Connect(fdc, c_fdc))
    return false;
  if (!fdc->Init(diskmgr, &scheduler_, &bus2, kPIRQ2, kPFDStat))
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  設定反映
//
void PC88::ApplyConfig(Config* cfg) {
  cfgflags = cfg->flags;
  cfgflag2 = cfg->flag2;

  base_->SetSwitch(cfg);
  scrn->ApplyConfig(cfg);
  mem1_->ApplyConfig(cfg);
  crtc_->ApplyConfig(cfg);
  fdc->ApplyConfig(cfg);
  beep_->EnableSING(!(cfg->flags & Config::kDisableSing));
  opn1_->SetFMMixMode(!!(cfg->flag2 & Config::kUseFMClock));
  opn1_->SetVolume(cfg);
  opn2_->SetFMMixMode(!!(cfg->flag2 & Config::kUseFMClock));
  opn2_->SetVolume(cfg);

  cpumode = (cfg->cpumode == Config::kMainSubAuto) ? (cfg->mainsubratio > 1 ? ms21 : ms11)
                                                   : (cfg->cpumode & 1);
  if ((cfg->flags & Config::kSubCPUControl) != 0)
    cpumode |= stopwhenidle;

  if (cfg->flags & PC8801::Config::kEnablePad) {
    joypad->SetButtonMode(cfg->flags & Config::kSwappedButtons ? JoyPad::SWAPPED : JoyPad::NORMAL);
  } else {
    joypad->SetButtonMode(JoyPad::DISABLED);
  }

  //  EnablePad((cfg->flags & PC8801::Config::enablepad) != 0);
  //  if (padenable)
  //      cfg->flags &= ~PC8801::Config::enablemouse;
  //  EnableMouse((cfg->flags & PC8801::Config::enablemouse) != 0);
}

// ---------------------------------------------------------------------------
//  音量変更
//
void PC88::SetVolume(PC8801::Config* cfg) {
  opn1_->SetVolume(cfg);
  opn2_->SetVolume(cfg);
}

// ---------------------------------------------------------------------------
//  1 フレーム分の時間を求める．
//
uint64_t PC88::GetFramePeriodNS() {
  return crtc_ ? crtc_->GetFramePeriodNS() : 1000000000ULL / 60;
}

// ---------------------------------------------------------------------------
//  仮想時間と現実時間の同期を取ったときに呼ばれる
//
void PC88::TimeSync() {
  bus1.Out(kPTimeSync, 0);
}

bool PC88::IsN80Supported() {
  return mem1_->IsN80Ready();
}

bool PC88::IsN80V2Supported() {
  return mem1_->IsN80V2Ready();
}
