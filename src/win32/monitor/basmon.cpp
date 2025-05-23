// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 2000.
// ---------------------------------------------------------------------------
//  $Id: basmon.cpp,v 1.1 2000/06/26 14:05:44 cisc Exp $

#include "win32/monitor/basmon.h"

#include <math.h>
#include <stdio.h>

#include "win32/resource.h"

using namespace pc8801;

// ---------------------------------------------------------------------------
//  構築/消滅
//
BasicMonitor::BasicMonitor() = default;

BasicMonitor::~BasicMonitor() = default;

bool BasicMonitor::Init(PC88* pc88) {
  if (!WinMonitor::Init(MAKEINTRESOURCE(IDD_BASMON)))
    return false;
  mv.Init(pc88);
  mv.SelectBank(services::MemoryViewer::kMainRam, services::MemoryViewer::kMainRam,
                services::MemoryViewer::kMainRam, services::MemoryViewer::kMainRam,
                services::MemoryViewer::kMainRam);
  bus = mv.GetBus();

  SetUpdateTimer(2000);

  nlines = 0;

  return true;
}

// ---------------------------------------------------------------------------
//  ダイアログ処理
//
BOOL BasicMonitor::DlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_INITDIALOG:
      Decode(true);
      break;

    case WM_TIMER:
      Decode(false);
      break;
  }
  return WinMonitor::DlgProc(hdlg, msg, wp, lp);
}

// ---------------------------------------------------------------------------
//  表示
//
void BasicMonitor::UpdateText() {
  int l = GetLine();
  for (int y = 0; y < GetHeight(); y++) {
    if (l < nlines) {
      Puts(basictext + line[l]);
    } else if (l == nlines)
      Puts("[EOB]");
    Puts("\n");
    l++;
  }
}

// ---------------------------------------------------------------------------
//  メモリから読み込み
//
inline uint32_t BasicMonitor::Read8(uint32_t addr) {
  return bus->Read8(addr);
}

inline uint32_t BasicMonitor::Read16(uint32_t addr) {
  return bus->Read8(addr) + bus->Read8(addr + 1) * 0x100;
}

inline uint32_t BasicMonitor::Read32(uint32_t addr) {
  return Read16(addr) + Read16(addr + 2) * 0x10000;
}

// ---------------------------------------------------------------------------
//  N88-BASIC 中間コードからテキストに変換
//
void BasicMonitor::Decode(bool always) {
  uint32_t src = Read16(0xe658);
  uint32_t end = Read16(0xeb18);

  if (!always && end - src == prvs)
    return;

  prvs = end - src;

  uint32_t link;
  char* text = basictext;
  int ln = 0;

  while (src < end && text < basictext + 0xfe00 && ln < 0x4000) {
    line[ln] = text - basictext;

    link = Read16(src);
    if (link < src || (link - src) > 0x102)
      break;

    uint32_t l = Read16(src + 2);
    text += sprintf(text, "%u ", l);
    src += 4;

    uint32_t c;
    while (c = Read8(src++)) {
      switch (c) {
        case 0x0b:
          text += sprintf(text, "&O%o", Read16(src)), src += 2;
          break;

        case 0x0c:
          text += sprintf(text, "&H%X", Read16(src)), src += 2;
          break;

        case 0x0d:
          text += sprintf(text, "%d", Read16(Read16(src) + 3)), src += 2;
          break;

        case 0x0e:
        case 0x1c:
          text += sprintf(text, "%d", Read16(src)), src += 2;
          break;

        case 0x0f:
          text += sprintf(text, "%d", Read8(src++));
          break;

        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1b:
          text += sprintf(text, "%d", c - 0x11);
          break;

        case 0x1d: {
          uint32_t f = Read32(src);
          int ma = (f & 0xffffff) | 0x800000;
          if (f & 0x800000)
            ma = -ma;
          double x = ldexp(ma / double(0x800000), ((f >> 24) & 0xff) - 129);
          double y;
          text += sprintf(text, modf(x, &y) ? "%.7g" : "%.7g!", x);
          src += 4;
        } break;

        case 0x1f: {
          uint32_t ma1 = Read32(src);
          uint32_t f = Read32(src + 4);
          int ma2 = (f & 0xffffff) | 0x800000;
          double ma = (ma2 + ma1 / (65536. * 65536.)) / 0x800000;
          if (f & 0x800000)
            ma = -ma;
          double x = ldexp(ma, ((f >> 24) & 0xff) - 129);
          src += 8;
          text += sprintf(text, "%.15g#", x);
        } break;

        case 0x84:
          memcpy(text, "DATA", 4);
          text += 4;
          for (; c = Read8(src), c && c != ':';) {
            src++;
            if (c == 0x22) {
              do {
                *text++ = c;
              } while (c = Read8(src++), c && c != 0x22);
              if (c == 0x22)
                *text++ = 0x22;
              else
                src--;
            } else
              *text++ = c;
          }
          break;

        case 0x22:
          do {
            *text++ = c;
          } while (c = Read8(src++), c && c != 0x22);
          if (c == 0x22)
            *text++ = 0x22;
          else
            src--;
          break;

        case 0x3a:
          if (Read8(src) == 0x9f)
            break;
          else if (Read8(src) == 0x8f) {
            *text++ = '\'';
            src += 2;
            for (; c = Read8(src); src++)
              *text++ = c;
          } else
            *text++ = c;
          break;

        case 0x8f:
          text += sprintf(text, "%s", rsvdword[c & 0x7f]);
          for (; c = Read8(src); src++)
            *text++ = c;
          break;

        case 0xaf:
          memcpy(text, "WHILE", 5);
          text += 5;
          if (Read8(src) == 0xf3)
            src++;
          break;

        default:
          if (c < 0x80)
            *text++ = c;
          else if (c < 0xff) {
            text += sprintf(text, "%s", rsvdword[c & 0x7f]);
          } else {
            text += sprintf(text, "%s", rsvdword[Read8(src++) | 0x80]);
          }
          break;
      }
    }
    *text++ = 0;
    ln++;
  }

  nlines = ln;
  SetLines(ln);
}

const char* BasicMonitor::rsvdword[] = {
    "",       "END",    "FOR",     "NEXT",      "DATA",   "INPUT",  "DIM",    "READ",   "LET",
    "GOTO",   "RUN",    "IF",      "RESTORE",   "GOSUB",  "RETURN", "REM",    "STOP",   "PRINT",
    "CLEAR",  "LIST",   "NEW",     "ON",        "WAIT",   "DEF",    "POKE",   "CONT",   "OUT",
    "LPRINT", "LLIST",  "CONSOLE", "WIDTH",     "ELSE",   "TRON",   "TROFF",  "SWAP",   "ERASE",
    "EDIT",   "ERROR",  "RESUME",  "DELETE",    "AUTO",   "RENUM",  "DEFSTR", "DEFINT", "DEFSNG",
    "DEFDBL", "LINE",   "WHILE",   "WEND",      "CALL",   "",       "",       "",       "WRITE",
    "COMMON", "CHAIN",  "OPTION",  "RANDOMIZE", "DSKO$",  "OPEN",   "FIELD",  "GET",    "PUT",
    "SET",    "CLOSE",  "LOAD",    "MERGE",     "FILES",  "NAME",   "KILL",   "LSET",   "RSET",
    "SAVE",   "LFILES", "MON",     "COLOR",     "CIRCLE", "COPY",   "CLS",    "PSET",   "PRESET",
    "PAINT",  "TERM",   "SCREEN",  "BLOAD",     "BSAVE",  "LOCATE", "BEEP",   "ROLL",   "HELP",
    "",       "KANJI",  "TO",      "THEN",      "TAB(",   "STEP",   "USR",    "FN",     "SPC(",
    "NOT",    "ERL",    "ERR",     "STRING$",   "USING",  "INSTR",  "'",      "VARPTR", "ATTR$",
    "DSKI$",  "SRQ",    "OFF",     "INKEY$",    ">",      "=",      "<",      "+",      "-",
    "*",      "/",      "^",       "AND",       "OR",     "XOR",    "EQV",    "IMP",    "MOD",
    "\\",     "",       "",        "LEFT$",     "RIGHT$", "MID$",   "SGN",    "INT",    "ABS",
    "SQR",    "RND",    "SIN",     "LOG",       "EXP",    "COS",    "TAN",    "ATN",    "FRE",
    "INP",    "POS",    "LEN",     "STR$",      "VAL",    "ASC",    "CHR$",   "PEEK",   "SPACE$",
    "OCT$",   "HEX$",   "LPOS",    "CINT",      "CSNG",   "CDBL",   "FIX",    "CVI",    "CVS",
    "CVD",    "EOF",    "LOC",     "LOF",       "FPOS",   "MKI$",   "MKS$",   "MKD$",   "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "DSKF",   "VIEW",    "WINDOW",    "POINT",  "CSRLIN", "MAP",    "SEARCH", "MOTOR",
    "PEN",    "DATE$",  "COM",     "KEY",       "TIME$",  "WBYTE",  "RBYTE",  "POLL",   "ISET",
    "IEEE",   "IRESET", "STATUS",  "CMD",       "",       "",       "",       "",       "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "",       "",        "",          "",       "",       "",       "",       "",
    "",       "",       "",        "",
};
