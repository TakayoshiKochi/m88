// ---------------------------------------------------------------------------
//  OPN/A/B interface with ADPCM support
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: opna.cpp,v 1.70 2004/02/06 13:13:39 cisc Exp $

#include "devices/opna.h"

#include <algorithm>

// #define BUILD_OPN
// #define BUILD_OPNA
#define BUILD_OPNB

namespace FM {

// ---------------------------------------------------------------------------
//  YM2610(OPNB)
// ---------------------------------------------------------------------------

#ifdef BUILD_OPNB

// ---------------------------------------------------------------------------
//  構築
//
OPNB::OPNB() {
  adpcmabuf = nullptr;
  adpcmasize = 0;
  for (int i = 0; i < 6; ++i) {
    adpcma[i].pan = 0;
    adpcma[i].level = 0;
    adpcma[i].volume = 0;
    adpcma[i].pos = 0;
    adpcma[i].step = 0;
    adpcma[i].volume = 0;
    adpcma[i].start = 0;
    adpcma[i].stop = 0;
    adpcma[i].adpcmx = 0;
    adpcma[i].adpcmd = 0;
  }
  adpcmatl = 0;
  adpcmakey = 0;
  adpcmatvol = 0;
  adpcm_mask_ = 0;
  adpcm_notice_ = 0x8000;
  granularity_ = -1;
  csm_ch_ = &ch[2];

  InitADPCMATable();
}

OPNB::~OPNB() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool OPNB::Init(uint32_t c,
                uint32_t r,
                bool ipflag,
                uint8_t* _adpcma,
                int _adpcma_size,
                uint8_t* _adpcmb,
                int _adpcmb_size) {
  int i;
  if (!SetRate(c, r, ipflag))
    return false;
  if (!OPNABase::Init(c, r, ipflag))
    return false;

  adpcmabuf = _adpcma;
  adpcmasize = _adpcma_size;
  adpcm_buf_ = _adpcmb;

  for (i = 0; i <= 24; i++)  // max 16M bytes
  {
    if (_adpcmb_size <= (1 << i)) {
      adpcm_mask_ = (1 << i) - 1;
      break;
    }
  }

  //  adpcmmask = _adpcmb_size - 1;
  limit_addr_ = adpcm_mask_;

  Reset();

  SetVolumeFM(0);
  SetVolumePSG(0);
  SetVolumeADPCMB(0);
  SetVolumeADPCMATotal(0);
  for (i = 0; i < 6; i++)
    SetVolumeADPCMA(i, 0);
  SetChannelMask(0);
  return true;
}

// ---------------------------------------------------------------------------
//  リセット
//
void OPNB::Reset() {
  OPNABase::Reset();

  stmask = ~0;
  adpcmakey = 0;
  reg29_ = ~0;

  for (int i = 0; i < 6; ++i) {
    adpcma[i].pan = 0;
    adpcma[i].level = 0;
    adpcma[i].volume = 0;
    adpcma[i].pos = 0;
    adpcma[i].step = 0;
    adpcma[i].volume = 0;
    adpcma[i].start = 0;
    adpcma[i].stop = 0;
    adpcma[i].adpcmx = 0;
    adpcma[i].adpcmd = 0;
  }
}

// ---------------------------------------------------------------------------
//  サンプリングレート変更
//
bool OPNB::SetRate(uint32_t c, uint32_t r, bool ipflag) {
  if (!OPNABase::SetRate(c, r, ipflag))
    return false;

  adpcmastep = int(double(c) / 54 * 8192 / r);
  return true;
}

// ---------------------------------------------------------------------------
//  レジスタアレイにデータを設定
//
void OPNB::SetReg(uint32_t addr, uint32_t data) {
  addr &= 0x1ff;

  switch (addr) {
    // omitted registers
    case 0x29:
    case 0x2d:
    case 0x2e:
    case 0x2f:
      break;

    // ADPCM A ---------------------------------------------------------------
    case 0x100:            // DM/KEYON
      if (!(data & 0x80))  // KEY ON
      {
        adpcmakey |= data & 0x3f;
        for (int c = 0; c < 6; c++) {
          if (data & (1 << c)) {
            ResetStatus(0x100 << c);
            adpcma[c].pos = adpcma[c].start;
            //                  adpcma[c].step = 0x10000 - adpcma[c].step;
            adpcma[c].step = 0;
            adpcma[c].adpcmx = 0;
            adpcma[c].adpcmd = 0;
            adpcma[c].nibble = 0;
          }
        }
      } else {  // DUMP
        adpcmakey &= ~data;
      }
      break;

    case 0x101:
      adpcmatl = ~data & 63;
      break;

    case 0x108:
    case 0x109:
    case 0x10a:
    case 0x10b:
    case 0x10c:
    case 0x10d:
      adpcma[addr & 7].pan = (data >> 6) & 3;
      adpcma[addr & 7].level = ~data & 31;
      break;

    case 0x110:
    case 0x111:
    case 0x112:  // START ADDRESS (L)
    case 0x113:
    case 0x114:
    case 0x115:
    case 0x118:
    case 0x119:
    case 0x11a:  // START ADDRESS (H)
    case 0x11b:
    case 0x11c:
    case 0x11d:
      adpcmareg[addr - 0x110] = data;
      adpcma[addr & 7].pos = adpcma[addr & 7].start =
          (adpcmareg[(addr & 7) + 8] * 256 + adpcmareg[addr & 7]) << 9;
      break;

    case 0x120:
    case 0x121:
    case 0x122:  // END ADDRESS (L)
    case 0x123:
    case 0x124:
    case 0x125:
    case 0x128:
    case 0x129:
    case 0x12a:  // END ADDRESS (H)
    case 0x12b:
    case 0x12c:
    case 0x12d:
      adpcmareg[addr - 0x110] = data;
      adpcma[addr & 7].stop = (adpcmareg[(addr & 7) + 24] * 256 + adpcmareg[(addr & 7) + 16] + 1)
                              << 9;
      break;

    // ADPCMB -----------------------------------------------------------------
    case 0x10:
      if ((data & 0x80) && !adpcm_playing_) {
        adpcm_playing_ = true;
        mem_addr_ = start_addr_;
        adpcm_x_ = 0, adpcm_d_ = 127;
        adplc_ = 0;
      }
      if (data & 1)
        adpcm_playing_ = false;
      control1_ = data & 0x91;
      break;

    case 0x11:  // Control Register 2
      control2_ = data & 0xc0;
      break;

    case 0x12:  // Start Address L
    case 0x13:  // Start Address H
      adpcmreg_[addr - 0x12 + 0] = data;
      start_addr_ = (adpcmreg_[1] * 256 + adpcmreg_[0]) << 9;
      mem_addr_ = start_addr_;
      break;

    case 0x14:  // Stop Address L
    case 0x15:  // Stop Address H
      adpcmreg_[addr - 0x14 + 2] = data;
      stop_addr_ = (adpcmreg_[3] * 256 + adpcmreg_[2] + 1) << 9;
      //      Log("  stopaddr %.6x", stopaddr);
      break;

    case 0x19:  // delta-N L
    case 0x1a:  // delta-N H
      adpcmreg_[addr - 0x19 + 4] = data;
      delta_n_ = adpcmreg_[5] * 256 + adpcmreg_[4];
      delta_n_ = std::max(256U, delta_n_);
      adpld_ = delta_n_ * adpl_base_ >> 16;
      break;

    case 0x1b:  // Level Control
      adpcm_level_ = data;
      adpcm_volume_ = (adpcm_vol_ * adpcm_level_) >> 12;
      break;

    case 0x1c:  // Flag Control
      stmask = ~((data & 0xbf) << 8);
      status &= stmask;
      UpdateStatus();
      break;

    default:
      OPNABase::SetReg(addr, data);
      break;
  }
  //  Log("\n");
}

// ---------------------------------------------------------------------------
//  レジスタ取得
//
uint32_t OPNB::GetReg(uint32_t addr) {
  if (addr < 0x10)
    return psg_.GetReg(addr);

  return 0;
}

// ---------------------------------------------------------------------------
//  拡張ステータスを読みこむ
//
uint32_t OPNB::ReadStatusEx() {
  return (status & stmask) >> 8;
}

// ---------------------------------------------------------------------------
//  YM2610
//
int OPNB::jedi_table[(48 + 1) * 16];

void OPNB::InitADPCMATable() {
  const static int8_t table2[] = {
      1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15,
  };

  for (int i = 0; i <= 48; i++) {
    int s = int(16.0 * pow(1.1, i) * 3);
    for (int j = 0; j < 16; j++) {
      jedi_table[i * 16 + j] = s * table2[j] / 8;
    }
  }
}

// ---------------------------------------------------------------------------
//  ADPCMA 合成
//
void OPNB::ADPCMAMix(Sample* buffer, uint32_t count) {
  const static int decode_tableA1[16] = {-1 * 16, -1 * 16, -1 * 16, -1 * 16, 2 * 16,  5 * 16,
                                         7 * 16,  9 * 16,  -1 * 16, -1 * 16, -1 * 16, -1 * 16,
                                         2 * 16,  5 * 16,  7 * 16,  9 * 16};

  if (adpcmatvol < 128 && (adpcmakey & 0x3f)) {
    Sample* limit = buffer + count * 2;
    for (int i = 0; i < 6; i++) {
      ADPCMA& r = adpcma[i];
      if ((adpcmakey & (1 << i)) && r.level < 128) {
        uint32_t maskl = r.pan & 2 ? -1 : 0;
        uint32_t maskr = r.pan & 1 ? -1 : 0;
        if (rhythmmask_ & (1 << i)) {
          maskl = maskr = 0;
        }

        int db = Limit(adpcmatl + adpcmatvol + r.level + r.volume, 127, -31);
        int vol = tltable[FM_TLPOS + (db << (FM_TLBITS - 7))] >> 4;

        Sample* dest = buffer;
        for (; dest < limit; dest += 2) {
          r.step += adpcmastep;
          if (r.pos >= r.stop) {
            SetStatus(0x100 << i);
            adpcmakey &= ~(1 << i);
            break;
          }

          for (; r.step > 0x10000; r.step -= 0x10000) {
            int data;
            if (!(r.pos & 1)) {
              r.nibble = adpcmabuf[r.pos >> 1];
              data = r.nibble >> 4;
            } else {
              data = r.nibble & 0x0f;
            }
            r.pos++;

            r.adpcmx += jedi_table[r.adpcmd + data];
            r.adpcmx = Limit(r.adpcmx, 2048 * 3 - 1, -2048 * 3);
            r.adpcmd += decode_tableA1[data];
            r.adpcmd = Limit(r.adpcmd, 48 * 16, 0);
          }
          int sample = (r.adpcmx * vol) >> 10;
          StoreSample(dest[0], sample & maskl);
          StoreSample(dest[1], sample & maskr);
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  音量設定
//
void OPNB::SetVolumeADPCMATotal(int db) {
  db = std::min(db, 20);
  adpcmatvol = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMA(int index, int db) {
  db = std::min(db, 20);
  adpcma[index].volume = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMB(int db) {
  db = std::min(db, 20);
  if (db > -192)
    adpcm_vol_ = int(65536.0 * pow(10.0, db / 40.0));
  else
    adpcm_vol_ = 0;
}

// ---------------------------------------------------------------------------
//  合成
//  in:     buffer      合成先
//          nsamples    合成サンプル数
//
void OPNB::Mix(Sample* buffer, int nsamples) {
  FMMix(buffer, nsamples);
  psg_.Mix(buffer, nsamples);
  ADPCMBMix(buffer, nsamples);
  ADPCMAMix(buffer, nsamples);
}

#endif  // BUILD_OPNB

}  // namespace FM
