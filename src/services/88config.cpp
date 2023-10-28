// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: 88config.cpp,v 1.28 2003/09/28 14:35:35 cisc Exp $

#include "services/88config.h"

#include "common/misc.h"

static const char* AppName = "M88p2 for Windows";

namespace services {

// ---------------------------------------------------------------------------
//  LoadConfigEntry
//
static bool LoadConfigEntry(const std::string_view inifile,
                            const char* entry,
                            int* value,
                            int dfault_value,
                            bool apply_default) {
  int n = GetPrivateProfileInt(AppName, entry, -1, inifile.data());

  if (n == -1 && apply_default)
    n = dfault_value;
  if (n != -1) {
    *value = n;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  LoadConfigDirectory
//
void LoadConfigDirectory(pc8801::Config* cfg,
                         const std::string_view inifile,
                         const char* entry,
                         bool readalways) {
  if (readalways || (cfg->flags & pc8801::Config::kSaveDirectory)) {
    char path[MAX_PATH];
    if (GetPrivateProfileString(AppName, entry, ";", path, MAX_PATH, inifile.data())) {
      if (path[0] != ';')
        SetCurrentDirectory(path);
    }
  }
}

// ---------------------------------------------------------------------------
//  M88Config::LoadConfig
//
#define VOLUME_BIAS 100

#define LOADVOLUMEENTRY(key, def, vol)                      \
  if (LoadConfigEntry(inifile, key, &n, def, applydefault)) \
    vol = n - VOLUME_BIAS;

void LoadConfig(pc8801::Config* cfg, const std::string_view inifile, bool applydefault) {
  int n;

  n = pc8801::Config::kSubCPUControl | pc8801::Config::kSaveDirectory | pc8801::Config::kEnableWait;
  LoadConfigEntry(inifile, "Flags", &cfg->flags, n, applydefault);
  cfg->flags &= ~pc8801::Config::kSpecialPalette;

  n = 0;
  LoadConfigEntry(inifile, "Flag2", &cfg->flag2, n, applydefault);
  cfg->flag2 &= ~(pc8801::Config::kMask0 | pc8801::Config::kMask1 | pc8801::Config::kMask2);

  if (LoadConfigEntry(inifile, "CPUClock", &n, 40, applydefault))
    cfg->legacy_clock = Limit(n, 1000, 1);

  //  if (LoadConfigEntry(inifile, "Speed", &n, 1000, applydefault))
  //      cfg->speed = Limit(n, 2000, 500);
  cfg->speed = 1000;

  // Obsolete
  // if (LoadConfigEntry(inifile, "RefreshTiming", &n, 1, applydefault))
  //   cfg->refreshtiming = Limit(n, 4, 1);

  if (LoadConfigEntry(inifile, "BASICMode", &n, static_cast<int>(pc8801::BasicMode::kN88V2),
                      applydefault)) {
    pc8801::BasicMode bm = static_cast<pc8801::BasicMode>(n);
    if (bm == pc8801::BasicMode::kN80 || bm == pc8801::BasicMode::kN88V1 ||
        bm == pc8801::BasicMode::kN88V1H || bm == pc8801::BasicMode::kN88V2 ||
        bm == pc8801::BasicMode::kN802 || bm == pc8801::BasicMode::kN80V2 ||
        bm == pc8801::BasicMode::kN88V2CD)
      cfg->basicmode = bm;
    else
      cfg->basicmode = pc8801::BasicMode::kN88V2;
  }

  if (LoadConfigEntry(inifile, "Sound", &n, 55467, applydefault)) {
    static const uint16_t srate[] = {0, 11025, 22050, 44100, 44100, 48000, 55467};
    if (n < 7)
      cfg->sound_output_hz = srate[n];
    else
      cfg->sound_output_hz = Limit(n, 192000, 8000);
  }

  // Obsolete
  // if (LoadConfigEntry(inifile, "OPNClock", &n, 3993600, applydefault))
  //   cfg->opnclock = Limit(n, 10000000, 1000000);

  if (LoadConfigEntry(inifile, "ERAMBank", &n, 4, applydefault))
    cfg->erambanks = Limit(n, 256, 0);

  if (LoadConfigEntry(inifile, "KeyboardType", &n, 0, applydefault))
    cfg->keytype = static_cast<pc8801::KeyboardType>(n);
  if (cfg->keytype == pc8801::KeyboardType::kPC98_obsolete)
    cfg->keytype = pc8801::KeyboardType::kAT106;

  if (LoadConfigEntry(inifile, "Switches", &n, 1829, applydefault))
    cfg->dipsw = n;

  if (LoadConfigEntry(inifile, "SoundBuffer", &n, 50, applydefault))
    cfg->sound_buffer_ms = Limit(n, 1000, 20);

  if (LoadConfigEntry(inifile, "MouseSensibility", &n, 4, applydefault))
    cfg->mousesensibility = Limit(n, 10, 1);

  if (LoadConfigEntry(inifile, "CPUMode", &n, pc8801::Config::kMainSubAuto, applydefault))
    cfg->cpumode = Limit(n, 2, 0);

  if (LoadConfigEntry(inifile, "LPFCutoff", &n, 8000, applydefault))
    cfg->lpffc = Limit(n, 24000, 3000);

  if (LoadConfigEntry(inifile, "LPFOrder", &n, 4, applydefault))
    cfg->lpforder = Limit(n, 16, 2);

  if (LoadConfigEntry(inifile, "ROMEOLatency", &n, 100, applydefault))
    cfg->romeolatency = Limit(n, 500, 0);

  LOADVOLUMEENTRY("VolumeFM", VOLUME_BIAS, cfg->volfm);
  LOADVOLUMEENTRY("VolumeSSG", 97, cfg->volssg);
  LOADVOLUMEENTRY("VolumeADPCM", VOLUME_BIAS, cfg->voladpcm);
  LOADVOLUMEENTRY("VolumeRhythm", VOLUME_BIAS, cfg->volrhythm);
  LOADVOLUMEENTRY("VolumeBD", VOLUME_BIAS, cfg->volbd);
  LOADVOLUMEENTRY("VolumeSD", VOLUME_BIAS, cfg->volsd);
  LOADVOLUMEENTRY("VolumeTOP", VOLUME_BIAS, cfg->voltop);
  LOADVOLUMEENTRY("VolumeHH", VOLUME_BIAS, cfg->volhh);
  LOADVOLUMEENTRY("VolumeTOM", VOLUME_BIAS, cfg->voltom);
  LOADVOLUMEENTRY("VolumeRIM", VOLUME_BIAS, cfg->volrim);

  LoadConfigEntry(inifile, "WinPosY", &cfg->winposy, 64, applydefault);
  LoadConfigEntry(inifile, "WinPosX", &cfg->winposx, 64, applydefault);
}

// ---------------------------------------------------------------------------
//  SaveEntry
//
static bool SaveEntry(const std::string_view inifile,
                      const char* entry,
                      int value,
                      bool applydefault) {
  char buf[MAX_PATH];
  if (applydefault || -1 != GetPrivateProfileInt(AppName, entry, -1, inifile.data())) {
    wsprintf(buf, "%d", value);
    WritePrivateProfileString(AppName, entry, buf, inifile.data());
    return true;
  }
  return false;
}

static bool SaveEntry(const std::string_view inifile,
                      const char* entry,
                      const char* value,
                      bool applydefault) {
  bool apply = applydefault;
  if (!applydefault) {
    char buf[8];
    GetPrivateProfileString(AppName, entry, ";", buf, sizeof(buf), inifile.data());
    if (buf[0] != ';')
      apply = true;
  }
  if (apply)
    WritePrivateProfileString(AppName, entry, value, inifile.data());
  return apply;
}

// ---------------------------------------------------------------------------
//  SaveConfig
//
void SaveConfig(pc8801::Config* cfg, const std::string_view inifile, bool writedefault) {
  char buf[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, buf);
  SaveEntry(inifile, "Directory", buf, writedefault);

  SaveEntry(inifile, "Flags", cfg->flags, writedefault);
  SaveEntry(inifile, "Flag2", cfg->flag2, writedefault);
  SaveEntry(inifile, "CPUClock", cfg->legacy_clock, writedefault);
  // SaveEntry(inifile, "Speed", cfg->speed, writedefault);
  // Obsolete - always refresh.
  // SaveEntry(inifile, "RefreshTiming", cfg->refreshtiming, writedefault);
  SaveEntry(inifile, "BASICMode", static_cast<int>(cfg->basicmode), writedefault);
  SaveEntry(inifile, "Sound", cfg->sound_output_hz, writedefault);
  SaveEntry(inifile, "Switches", cfg->dipsw, writedefault);
  SaveEntry(inifile, "SoundBuffer", cfg->sound_buffer_ms, writedefault);
  SaveEntry(inifile, "MouseSensibility", cfg->mousesensibility, writedefault);
  SaveEntry(inifile, "CPUMode", cfg->cpumode, writedefault);
  SaveEntry(inifile, "KeyboardType", static_cast<int>(cfg->keytype), writedefault);
  SaveEntry(inifile, "ERAMBank", cfg->erambanks, writedefault);
  SaveEntry(inifile, "LPFCutoff", cfg->lpffc, writedefault);
  SaveEntry(inifile, "LPFOrder", cfg->lpforder, writedefault);
  SaveEntry(inifile, "ROMEOLatency", cfg->romeolatency, writedefault);

  SaveEntry(inifile, "VolumeFM", cfg->volfm + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeSSG", cfg->volssg + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeADPCM", cfg->voladpcm + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeRhythm", cfg->volrhythm + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeBD", cfg->volbd + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeSD", cfg->volsd + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeTOP", cfg->voltop + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeHH", cfg->volhh + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeTOM", cfg->voltom + VOLUME_BIAS, writedefault);
  SaveEntry(inifile, "VolumeRIM", cfg->volrim + VOLUME_BIAS, writedefault);

  SaveEntry(inifile, "WinPosY", cfg->winposy, writedefault);
  SaveEntry(inifile, "WinPosX", cfg->winposx, writedefault);
}

}  // namespace services
