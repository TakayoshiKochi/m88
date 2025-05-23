//----------------------------------------------------------------------
//	SCCI Sound Interfaces defines
//----------------------------------------------------------------------
#pragma once

// Sound chip list
enum {
  SC_TYPE_NONE = 0,
  SC_TYPE_YM2608,
  SC_TYPE_YM2151,
  SC_TYPE_YM2610,
  SC_TYPE_YM2203,
  SC_TYPE_YM2612,
  SC_TYPE_AY8910,
  SC_TYPE_SN76489,
  SC_TYPE_YM3812,
  SC_TYPE_YMF262,
  SC_TYPE_YM2413,
  SC_TYPE_YM3526,
  SC_TYPE_YMF288,
  SC_TYPE_SCC,
  SC_TYPE_SCCS,
  SC_TYPE_Y8950,
  SC_TYPE_MAX
};

// Sound chip clock list
enum {
  SC_CLOCK_3579545 = 3579545,    // SSG,OPN,OPM,SN76489 etc
  SC_CLOCK_3993600 = 3993600,    // OPN(88)
  SC_CLOCK_4000000 = 4000000,    // SSF,OPN,OPM etc
  SC_CLOCK_7670454 = 7670454,    // YM-2612 etc
  SC_CLOCK_7987200 = 7987200,    // OPNA(88)
  SC_CLOCK_8000000 = 8000000,    // OPNB etc
  SC_CLOCK_12500000 = 12500000,  // RF5C164
  SC_CLOCK_14318180 = 14318180,  // OPL2
  SC_CLOCK_16934400 = 16934400,  // YMF271
  SC_CLOCK_23011361 = 23011361,  // PWM
};

// Sound chip location
enum {
  SC_SC_LOCATION_MONO = 0,
  SC_SC_LOCATION_LEFT = 1,
  SC_SC_LOCATION_RIGHT = 2,
  SC_SC_LOCATION_STEREO = 3
};
