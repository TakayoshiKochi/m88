//
//  Windows 系 includes
//  すべての標準ヘッダーを含む
//
//  $Id: headers.h,v 1.13 2003/05/12 22:26:35 cisc Exp $
//

#pragma once

#define STRICT
#define WIN32_LEAN_AND_MEAN

// #define DIRECTSOUND_VERSION 0x500  // for pre-DirectX7 environment

//#pragma warning(disable : 4786)
//#pragma warning(disable : 4996)

#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <dsound.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <process.h>
#include <assert.h>
#include <time.h>

#include "win32/types.h"

using namespace std;

// --- STL 関係

#ifdef _MSC_VER
#undef max
#define max _MAX
#undef min
#define min _MIN
#endif
