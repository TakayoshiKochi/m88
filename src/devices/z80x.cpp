#include "devices/z80x.h"

void CPUExecutorX::Init() {
  clock_count_ = 0;
  exec_clocks_ = 0;
  eshift_ = 0;
}

// ---------------------------------------------------------------------------
// 命令遂行
//
// static
int64_t CPUExecutorX::ExecSingle(Z80X* first, Z80X* second, int clocks) {
  int64_t c = first->GetClocks();

  currentcpu = first;
  first->start_count_ = first->delay_count_ = c;
  first->SingleStep();

  cbase = c;
  first->Exec0(first, c + clocks, c);

  c = first->GetClocks();
  second->exec_clocks_ = c;
  second->clock_count_ = 0;

  return c - cbase;
}

// ---------------------------------------------------------------------------
// 2CPU 実行
//
// static
int64_t CPUExecutorX::ExecDual(Z80X* first, Z80X* second, int count) {
  currentcpu = second;
  second->start_count_ = second->delay_count_ = first->GetClocks();
  second->SingleStep();
  currentcpu = first;
  first->start_count_ = first->delay_count_ = second->GetClocks();
  first->SingleStep();

  int64_t c1 = first->GetClocks();
  int64_t c2 = second->GetClocks();
  int64_t delay = c2 - c1;
  cbase = delay > 0 ? c1 : c2;
  int64_t stop = cbase + count;

  while ((stop - first->GetClocks() > 0) || (stop - second->GetClocks() > 0)) {
    stop = first->Exec0(first, stop, second->GetClocks());
    stop = second->Exec0(second, stop, first->GetClocks());
  }
  return stop - cbase;
}

// ---------------------------------------------------------------------------
// 2CPU 実行
//
// static
int64_t CPUExecutorX::ExecDual2(Z80X* first, Z80X* second, int count) {
  currentcpu = second;
  second->start_count_ = second->delay_count_ = first->GetClocks();
  second->SingleStep();
  currentcpu = first;
  first->start_count_ = first->delay_count_ = second->GetClocks();
  first->SingleStep();

  int64_t c1 = first->GetClocks();
  int64_t c2 = second->GetClocks();
  int64_t delay = c2 - c1;
  cbase = delay > 0 ? c1 : c2;
  int64_t stop = cbase + count;

  while ((stop - first->GetClocks() > 0) || (stop - second->GetClocks() > 0)) {
    stop = first->Exec0(first, stop, second->GetClocks());
    stop = second->Exec1(second, stop, first->GetClocks());
  }
  return stop - cbase;
}

// ---------------------------------------------------------------------------
// 片方実行
//
int64_t CPUExecutorX::Exec0(Z80X* cpu, int64_t stop, int64_t other) {
  int64_t clocks = stop - GetClocks();
  if (clocks > 0) {
    eshift_ = 0;
    currentcpu = cpu;
    stop_count_ = stop;
    delay_count_ = other;
    exec_clocks_ += clock_count_ + clocks;

    for (clock_count_ = -clocks; clock_count_ < 0;)
      cpu->SingleStep();
    currentcpu = nullptr;
    return stop_count_;
  } else {
    return stop;
  }
}

// ---------------------------------------------------------------------------
// 片方実行
//
int64_t CPUExecutorX::Exec1(Z80X* cpu, int64_t stop, int64_t other) {
  int64_t clocks = stop - GetClocks();
  if (clocks > 0) {
    eshift_ = 1;
    currentcpu = cpu;
    stop_count_ = stop;
    delay_count_ = other;
    exec_clocks_ += clock_count_ * 2 + clocks;
    for (clock_count_ = -clocks / 2; clock_count_ < 0;) {
      cpu->SingleStep();
    }
    currentcpu = nullptr;
    return stop_count_;
  } else {
    return stop;
  }
}

// ---------------------------------------------------------------------------
// 同期チェック
//
bool CPUExecutorX::Sync() {
  // もう片方のCPUよりも遅れているか？
  if (GetClocks() - delay_count_ <= 1)
    return true;
  // 進んでいた場合 Exec0 を抜ける
  exec_clocks_ += clock_count_ << eshift_;
  clock_count_ = 0;
  return false;
}

// ---------------------------------------------------------------------------
// Exec を途中で中断
//
void CPUExecutorX::Stop(int count) {
  exec_clocks_ = stop_count_ = GetClocks() + count;
  clock_count_ = -count >> eshift_;
}

// static
void CPUExecutorX::StopDual(int count) {
  if (currentcpu)
    currentcpu->Stop(count);
}

Z80X* CPUExecutorX::currentcpu = nullptr;
int64_t CPUExecutorX::cbase = 0;

// static
uint8_t Z80X::ZRead8(void* ctx, uint16_t addr) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  return self->Read8(addr);
}

// static
void Z80X::ZWrite8(void* ctx, uint16_t addr, uint8_t data) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  self->Write8(addr, data);
}

// static
uint8_t Z80X::ZIn(void* ctx, uint16_t addr) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  if (self->IsSyncPort(addr & 0xff))
    self->Sync();
  return self->Inp(addr);
}

// static
void Z80X::ZOut(void* ctx, uint16_t addr, uint8_t data) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  if (self->IsSyncPort(addr & 0xff))
    self->Sync();
  self->Outp(addr, data);
}

// static
uint8_t Z80X::ZIntAck(void* ctx, uint16_t addr) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  return self->bus_->In(self->int_ack_);
}

// static
void Z80X::ZHalt(void* ctx, uint8_t signal) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  // TODO: implement
}

// static
void Z80X::ZNotify(void* ctx) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  // TODO: implement
}

// static
uint8_t Z80X::ZIllegal(void* ctx, uint8_t op) {
  auto* self = reinterpret_cast<Z80X*>(ctx);
  return 0;
}

Z80X::Z80X(const ID& id) : Device(id), IOStrategy(), MemStrategy(), CPUExecutorX(this) {
  z80_.cycles = 0;
  z80_.cycle_limit = Z80_MAXIMUM_CYCLES_PER_STEP;
  z80_.context = (void*)this;
  z80_.fetch_opcode = &Z80X::ZRead8;
  z80_.fetch = &Z80X::ZRead8;
  z80_.read = &Z80X::ZRead8;
  z80_.write = &Z80X::ZWrite8;
  z80_.in = &Z80X::ZIn;
  z80_.out = &Z80X::ZOut;
  z80_.halt = &Z80X::ZHalt;
  z80_.nop = nullptr;
  z80_.nmia = nullptr;
  z80_.inta = &Z80X::ZIntAck;
  z80_.int_fetch = &Z80X::ZRead8;
  // notifications
  z80_.ld_i_a = nullptr;
  z80_.ld_r_a = nullptr;
  z80_.reti = nullptr;
  z80_.retn = nullptr;
  // read hook
  z80_.hook = nullptr;
  z80_.illegal = &Z80X::ZIllegal;
}

Z80X::~Z80X() {}

bool Z80X::Init(MemoryManager* mem, IOBus* bus, int iack) {
  SetIOBus(bus);
  bus_ = bus;
  int_ack_ = iack;
  CPUExecutorX::Init();
  diag_.Init(mem);
  Reset();
  return true;
}

// ---------------------------------------------------------------------------
// リセット
//
void IOCALL Z80X::Reset(uint32_t, uint32_t) {
  z80_power(&z80_, true);
  ResetMemory();

  reg_.intmode = 0;  // IM0
  SetPC(0);          // pc, sp = 0
  wait_state_ = 0;

  CPUExecutorX::Reset();
}

void CPUExecutorX::Reset() {
  exec_clocks_ = 0;
}

void IOCALL Z80X::IRQ(uint32_t, uint32_t d) {
  z80_int(&z80_, d);
}

void IOCALL Z80X::NMI(uint32_t, uint32_t) {
  z80_nmi(&z80_);
}

void Z80X::Wait(bool wait) {
  if (wait)
    wait_state_ |= 2;
  else
    wait_state_ &= ~2;
}

void Z80X::SingleStep() {
  clock_count_ += z80_run(&z80_, 4);
}

void Z80X::ImportReg() {
  reg_.r.w.af = Z80_AF(z80_);
  reg_.r.w.hl = Z80_HL(z80_);
  reg_.r.w.de = Z80_DE(z80_);
  reg_.r.w.bc = Z80_BC(z80_);
  reg_.r.w.ix = Z80_IX(z80_);
  reg_.r.w.iy = Z80_IY(z80_);
  reg_.r.w.sp = Z80_SP(z80_);
  reg_.r_af = Z80_AF_(z80_);
  reg_.r_hl = Z80_HL_(z80_);
  reg_.r_de = Z80_DE_(z80_);
  reg_.r_bc = Z80_BC_(z80_);
  reg_.pc = Z80_PC(z80_);
  reg_.ireg = z80_.i;
  reg_.rreg = z80_.r;
  reg_.rreg7 = z80_.r7;
  reg_.intmode = z80_.im;
  reg_.iff1 = z80_.iff1;
  reg_.iff2 = z80_.iff2;
}

void Z80X::ExportReg() {
  Z80_AF(z80_) = reg_.r.w.af;
  Z80_HL(z80_) = reg_.r.w.hl;
  Z80_DE(z80_) = reg_.r.w.de;
  Z80_BC(z80_) = reg_.r.w.bc;
  Z80_IX(z80_) = reg_.r.w.ix;
  Z80_IY(z80_) = reg_.r.w.iy;
  Z80_SP(z80_) = reg_.r.w.sp;
  Z80_AF_(z80_) = reg_.r_af;
  Z80_HL_(z80_) = reg_.r_hl;
  Z80_DE_(z80_) = reg_.r_de;
  Z80_BC_(z80_) = reg_.r_bc;
  Z80_PC(z80_) = reg_.pc;
  z80_.i = reg_.ireg;
  z80_.r = reg_.rreg;
  z80_.r7 = reg_.rreg7;
  z80_.im = reg_.intmode;
  z80_.iff1 = reg_.iff1;
  z80_.iff2 = reg_.iff2;
}

uint32_t IFCALL Z80X::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL Z80X::SaveStatus(uint8_t* s) {
  auto* st = (Status*)s;
  st->rev = ssrev;

  ImportReg();
  st->reg = reg_;

  st->z80_request = z80_.request;
  st->z80_resume = z80_.resume;
  st->z80_q = z80_.q;
  st->z80_options = z80_.options;
  st->z80_int_line = z80_.int_line;
  st->z80_halt_line = z80_.halt_line;

  st->wait = wait_state_;
  st->execcount = exec_clocks_;

  return true;
}

bool IFCALL Z80X::LoadStatus(const uint8_t* s) {
  const auto* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;
  reg_ = st->reg;
  ExportReg();
  SetPC(reg_.pc);

  z80_.request = st->z80_request;
  z80_.resume = st->z80_resume;
  z80_.q = st->z80_q;
  z80_.options = st->z80_options;
  z80_.int_line = st->z80_int_line;
  z80_.halt_line = st->z80_halt_line;

  wait_state_ = st->wait;
  exec_clocks_ = st->execcount;
  return true;
}

// ---------------------------------------------------------------------------
// Device descriptor
//
const Device::Descriptor Z80X::descriptor = {nullptr, outdef};

const Device::OutFuncPtr Z80X::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Z80X::Reset),
    static_cast<Device::OutFuncPtr>(&Z80X::IRQ),
    static_cast<Device::OutFuncPtr>(&Z80X::NMI),
};
