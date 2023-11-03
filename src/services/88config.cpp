// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: 88config.cpp,v 1.28 2003/09/28 14:35:35 cisc Exp $

#include "services/88config.h"

#include "common/misc.h"

static const char* AppName = "M88p2 for Windows";

namespace services {

namespace {
char m88ini[MAX_PATH];

void InitPathInfo() {
  char buf[MAX_PATH];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];

  GetModuleFileName(nullptr, buf, MAX_PATH);
  _splitpath(buf, drive, dir, fname, ext);
  sprintf(m88ini, "%s%s%s.ini", drive, dir, fname);
}

bool LoadConfigEntry(const std::string_view inifile,
                     const char* entry,
                     int* value,
                     int default_value) {
  int n = GetPrivateProfileInt(AppName, entry, -1, inifile.data());

  if (n == -1)
    n = default_value;
  if (n != -1) {
    *value = n;
    return true;
  }
  return false;
}

bool LoadConfigEntryU(const std::string_view inifile,
                      const char* entry,
                      uint32_t* value,
                      uint32_t default_value) {
  int n = GetPrivateProfileInt(AppName, entry, -1, inifile.data());
  if (n == -1)
    *value = default_value;
  if (n != -1) {
    *value = static_cast<uint32_t>(n);
    return true;
  }
  return false;
}

std::string LoadConfigString(const std::string_view inifile, const char* entry) {
  char str[256];
  if (GetPrivateProfileString(AppName, entry, "", str, 256, inifile.data())) {
    return {str};
  }
  return {};
}

bool SaveEntry(const std::string_view inifile, const char* entry, int value) {
  char buf[MAX_PATH];
  wsprintf(buf, "%d", value);
  WritePrivateProfileString(AppName, entry, buf, inifile.data());
  return true;
}

bool SaveEntry(const std::string_view inifile, const char* entry, const char* value) {
  WritePrivateProfileString(AppName, entry, value, inifile.data());
  return true;
}

// TODO:
constexpr int VOLUME_BIAS = 100;

void SaveConfig(const pc8801::Config* cfg, const std::string_view inifile) {
  char buf[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, buf);
  SaveEntry(inifile, "Directory", buf);

  SaveEntry(inifile, "Flags", cfg->flags);
  SaveEntry(inifile, "Flag2", cfg->flag2);
  SaveEntry(inifile, "CPUClock", cfg->legacy_clock);
  // SaveEntry(inifile, "Speed", cfg->speed);
  // Obsolete - always refresh.
  // SaveEntry(inifile, "RefreshTiming", cfg->refreshtiming);
  SaveEntry(inifile, "BASICMode", static_cast<int>(cfg->basicmode));
  SaveEntry(inifile, "Sound", cfg->sound_output_hz);
  SaveEntry(inifile, "Switches", cfg->dipsw);
  SaveEntry(inifile, "SoundBuffer", cfg->sound_buffer_ms);
  SaveEntry(inifile, "MouseSensibility", cfg->mousesensibility);
  SaveEntry(inifile, "CPUMode", cfg->cpumode);
  SaveEntry(inifile, "KeyboardType", static_cast<int>(cfg->keytype));
  SaveEntry(inifile, "ERAMBank", cfg->erambanks);

  // Obsolete
  // SaveEntry(inifile, "LPFCutoff", cfg->lpffc);
  // SaveEntry(inifile, "LPFOrder", static_cast<int>(cfg->lpforder));

  SaveEntry(inifile, "ROMEOLatency", cfg->romeolatency);

  SaveEntry(inifile, "VolumeFM", cfg->volfm + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeSSG", cfg->volssg + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeADPCM", cfg->voladpcm + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeRhythm", cfg->volrhythm + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeBD", cfg->volbd + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeSD", cfg->volsd + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeTOP", cfg->voltop + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeHH", cfg->volhh + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeTOM", cfg->voltom + VOLUME_BIAS);
  SaveEntry(inifile, "VolumeRIM", cfg->volrim + VOLUME_BIAS);

  SaveEntry(inifile, "WinPosY", cfg->winposy);
  SaveEntry(inifile, "WinPosX", cfg->winposx);

  SaveEntry(inifile, "SoundDriverType", static_cast<int>(cfg->sound_driver_type));
  SaveEntry(inifile, "PreferredASIODriver", cfg->preferred_asio_driver.c_str());
  SaveEntry(inifile, "UsePiccolo", cfg->flag2 & pc8801::Config::kUsePiccolo);
}

#define LOADVOLUMEENTRY(key, def, vol)        \
  if (LoadConfigEntry(inifile, key, &n, def)) \
    vol = n - VOLUME_BIAS;

void LoadConfig(pc8801::Config* cfg, const std::string_view inifile) {
  uint32_t u =
      pc8801::Config::kSubCPUControl | pc8801::Config::kSaveDirectory | pc8801::Config::kEnableWait;
  LoadConfigEntryU(inifile, "Flags", &cfg->flags, u);
  cfg->flags &= ~pc8801::Config::kSpecialPalette;

  u = 0;
  LoadConfigEntryU(inifile, "Flag2", &cfg->flag2, u);
  cfg->flag2 &= ~(pc8801::Config::kMask0 | pc8801::Config::kMask1 | pc8801::Config::kMask2);

  int n = 0;
  if (LoadConfigEntry(inifile, "CPUClock", &n, 40))
    cfg->legacy_clock = Limit(n, 1000, 1);

  //  if (LoadConfigEntry(inifile, "Speed", &n, 1000))
  //      cfg->speed = Limit(n, 2000, 500);
  cfg->speed = 1000;

  // Obsolete
  // if (LoadConfigEntry(inifile, "RefreshTiming", &n, 1))
  //   cfg->refreshtiming = Limit(n, 4, 1);

  if (LoadConfigEntry(inifile, "BASICMode", &n, static_cast<int>(pc8801::BasicMode::kN88V2))) {
    pc8801::BasicMode bm = static_cast<pc8801::BasicMode>(n);
    if (bm == pc8801::BasicMode::kN80 || bm == pc8801::BasicMode::kN88V1 ||
        bm == pc8801::BasicMode::kN88V1H || bm == pc8801::BasicMode::kN88V2 ||
        bm == pc8801::BasicMode::kN802 || bm == pc8801::BasicMode::kN80V2 ||
        bm == pc8801::BasicMode::kN88V2CD)
      cfg->basicmode = bm;
    else
      cfg->basicmode = pc8801::BasicMode::kN88V2;
  }

  if (LoadConfigEntry(inifile, "SoundDriverType", &n, 0)) {
    if (n >= pc8801::Config::SoundDriverType::kNumSoundDriverTypes)
      cfg->sound_driver_type = pc8801::Config::SoundDriverType::kAuto;
    else
      cfg->sound_driver_type = static_cast<pc8801::Config::SoundDriverType>(n);
  }

  cfg->preferred_asio_driver = LoadConfigString(inifile, "PreferredASIODriver");

  if (LoadConfigEntry(inifile, "Sound", &n, 55467)) {
    static const uint32_t srate[] = {0, 11025, 22050, 44100, 44100, 48000, 55467};
    if (n < 7)
      cfg->sound_output_hz = srate[n];
    else
      cfg->sound_output_hz = Limit(n, 192000, 8000);
  }

  // Obsolete
  // if (LoadConfigEntry(inifile, "OPNClock", &n, 3993600))
  //   cfg->opnclock = Limit(n, 10000000, 1000000);

  if (LoadConfigEntry(inifile, "ERAMBank", &n, 4))
    cfg->erambanks = Limit(n, 256, 0);

  if (LoadConfigEntry(inifile, "KeyboardType", &n, 0))
    cfg->keytype = static_cast<pc8801::KeyboardType>(n);
  if (cfg->keytype == pc8801::KeyboardType::kPC98_obsolete)
    cfg->keytype = pc8801::KeyboardType::kAT106;

  if (LoadConfigEntry(inifile, "Switches", &n, 1829))
    cfg->dipsw = n;

  if (LoadConfigEntry(inifile, "SoundBuffer", &n, 50))
    cfg->sound_buffer_ms = Limit(n, 1000, 20);

  if (LoadConfigEntry(inifile, "MouseSensibility", &n, 4))
    cfg->mousesensibility = Limit(n, 10, 1);

  if (LoadConfigEntry(inifile, "CPUMode", &n, pc8801::Config::kMainSubAuto))
    cfg->cpumode = Limit(n, 2, 0);

  //  if (LoadConfigEntry(inifile, "LPFCutoff", &n, 8000))
  //    cfg->lpffc = Limit(n, 24000, 3000);
  //
  //  if (LoadConfigEntry(inifile, "LPFOrder", &n, 4))
  //    cfg->lpforder = Limit(n, 16, 2);

  if (LoadConfigEntry(inifile, "ROMEOLatency", &n, 100))
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

  LoadConfigEntry(inifile, "WinPosY", &cfg->winposy, 64);
  LoadConfigEntry(inifile, "WinPosX", &cfg->winposx, 64);

  cfg->flag2 &= ~pc8801::Config::kUsePiccolo;
  if (LoadConfigEntry(inifile, "UsePiccolo", &n, 0)) {
    if (n)
      cfg->flag2 |= pc8801::Config::kUsePiccolo;
  }
}
}  // namespace

// static
ConfigService ConfigService::instance_;
// static
std::once_flag ConfigService::once_;

// static
void ConfigService::Init() {
  InitPathInfo();
  LoadConfig(&instance_.config_, m88ini);
}

void ConfigService::Save() {
  SaveConfig(&config_, m88ini);
}

void ConfigService::LoadConfigDirectory(const char* entry, bool readalways) {
  if (readalways || (config_.flags & pc8801::Config::kSaveDirectory)) {
    char path[MAX_PATH];
    if (GetPrivateProfileString(AppName, entry, ";", path, MAX_PATH, m88ini)) {
      if (path[0] != ';')
        SetCurrentDirectory(path);
    }
  }
}

}  // namespace services
