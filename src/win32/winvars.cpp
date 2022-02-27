//  $Id: winvars.cpp,v 1.1 2000/11/02 12:43:52 cisc Exp $

#include "win32/headers.h"
#include "win32/winvars.h"

static WinVars winvars;

int WinVars::var[nparam];

void WinVars::Init() {
  // Windows のバージョン
  OSVERSIONINFO vi;
  memset(&vi, 0, sizeof(vi));
  vi.dwOSVersionInfoSize = sizeof(vi);
  GetVersionEx(&vi);
  var[MajorVer] = vi.dwMajorVersion;
  var[MinorVer] = vi.dwMinorVersion;

  // 構造体とか
  var[OFNSIZE] = sizeof(OPENFILENAME);
  var[MIISIZE] = sizeof(MENUITEMINFO);
}
