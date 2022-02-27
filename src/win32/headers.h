//
//  Windows 系 includes
//  すべての標準ヘッダーを含む
//
//  $Id: headers.h,v 1.13 2003/05/12 22:26:35 cisc Exp $
//

#pragma once

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// #define DIRECTSOUND_VERSION 0x500  // for pre-DirectX7 environment

//#pragma warning(disable : 4786)
//#pragma warning(disable : 4996)

#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <dsound.h>
#include <commctrl.h>
