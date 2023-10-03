// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: aspi.cpp,v 1.1 1999/08/26 08:04:36 cisc Exp $

#include "aspi.h"

#include "aspidef.h"

// --------------------------------------------------------------------------
//  構築
//
ASPI::ASPI() {
  hmod_ = (HMODULE)LoadLibrary("wnaspi32.dll");

  n_hostadapters_ = 0;
  if (ConnectAPI()) {
    uint32_t s = (*pgasi_)();
    if ((s & 0xff00) == 0x0100) {
      n_hostadapters_ = s & 0xff;
    }
  }
}

// --------------------------------------------------------------------------
//  破棄
//
ASPI::~ASPI() {
  if (hmod_)
    FreeLibrary(hmod_);
}

// --------------------------------------------------------------------------
//  SendASPI32Command
//
inline uint32_t ASPI::SendCommand(void* srb) {
  return (*psac_)(srb);
}

// --------------------------------------------------------------------------
//
inline uint32_t ASPI::SendCommandAndWait(void* srb) {
  uint32_t result = SendCommand(srb);

  volatile SRB_Header* h = (SRB_Header*)srb;
  while (h->status == SS_PENDING)
    Sleep(2);

  return h->status;
}

// --------------------------------------------------------------------------
//  wnaspi32.dll をインポート
//
bool ASPI::ConnectAPI() {
  psac_ = nullptr;
  pgasi_ = nullptr;
  HMODULE hmod = GetModuleHandle("wnaspi32.dll");
  if (!hmod) {
    return false;
  }

  pgasi_ = (uint32_t(__cdecl*)())(GetProcAddress(hmod, "GetASPI32SupportInfo"));
  psac_ = (uint32_t(__cdecl*)(void*))(GetProcAddress(hmod, "SendASPI32Command"));

  return pgasi_ && psac_;
}

// --------------------------------------------------------------------------
//  HA_INQUIRY test
//
bool ASPI::InquiryAdapter(uint32_t ha, uint32_t* maxid, uint32_t* maxxfer) {
  if (ha < n_hostadapters_) {
    SRB_HAInquiry srb;
    memset(&srb, 0, sizeof(srb));

    srb.command = SC_HA_INQUIRY;
    srb.haid = ha;

    if (SS_COMPLETE == SendCommandAndWait(&srb)) {
      if (maxid)
        *maxid = srb.param_maxid ? srb.param_maxid : 8;
      if (maxxfer)
        *maxxfer = srb.param_maxtransfer;
      return true;
    }
  }
  return false;
}

// --------------------------------------------------------------------------
//  SC_GET_DEV_TYPE
//
int ASPI::GetDeviceType(uint32_t ha, uint32_t id, uint32_t lun) {
  SRB_GetDeviceBlock srb;
  memset(&srb, 0, sizeof(srb));

  srb.command = SC_GET_DEV_TYPE;
  srb.haid = ha;
  srb.targetid = id;
  srb.targetlun = lun;

  if (SS_COMPLETE == SendCommandAndWait(&srb))
    return srb.type;
  else
    return -1;
}

// --------------------------------------------------------------------------
//  SC_EXEC_SCSI_CMD
//
int ASPI::ExecuteSCSICommand(uint32_t ha,
                             uint32_t id,
                             uint32_t lun,
                             void* cdb,
                             uint32_t cdblen,
                             uint32_t dir,
                             void* data,
                             uint32_t datalen) {
  SRB_ExecuteIO srb;
  memset(&srb, 0, sizeof(srb));

  HANDLE hevent = CreateEvent(0, false, false, 0);

  srb.command = SC_EXEC_SCSI_CMD;
  srb.haid = ha;
  srb.targetid = id;
  srb.targetlun = lun;
  srb.flags = dir | SRB_EVENT_NOTIFY;
  srb.postproc = (void*)hevent;

  srb.senselen = SENSE_LEN;
  if (dir & (SRB_DIR_IN | SRB_DIR_OUT)) {
    srb.dataptr = (uint8_t*)data;
    srb.datalen = datalen;
  }
  srb.CDBlen = cdblen;
  memcpy(srb.CDB, cdb, cdblen);

  int r = SendCommand(&srb);
  if (r == SS_PENDING) {
    if (WAIT_TIMEOUT == WaitForSingleObject(hevent, 10000))
      AbortService(ha, &srb);
    r = srb.status;
  }
  CloseHandle(hevent);

  if (r == SS_COMPLETE) {
    if (srb.hastatus != 0)
      return ~0x1ff | srb.hastatus;
    return srb.targetstatus;
  } else {
    return ~0xff | srb.targetstatus;
  }
}

// --------------------------------------------------------------------------
//  SC_ABORT_SRB
//
void ASPI::AbortService(uint32_t ha, void* asrb) {
  SRB_Abort srb;
  memset(&srb, 0, sizeof(srb));

  srb.command = SC_ABORT_SRB;
  srb.haid = ha;
  srb.service = asrb;
  SendCommand(&srb);
}
