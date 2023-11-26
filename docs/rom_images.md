# ROM Images

## Mandatory ROMs

### `PC88.ROM` : Composite ROM (112KB)

The original format was defined for P88SR, and is supported by many other emulators.
The format was described at http://www1.plala.or.jp/aoto/tech.htm but the site
has been closed. You can find the page via the Wayback Machine.

Other emulators (M88, quasi88 etc.) support alternative to provide each part of the ROM separately.

| offset   | size | Description               | Alternative            |
|----------|------|---------------------------|------------------------|
| 0x00000  | 32KB | N88-BASIC                 | N88.ROM                |
| 0x08000  | 8KB  | N-BASIC (0x6000-0x7fff)   | N80.ROM, N88N.ROM      |
| 0x0a000  | 8KB  | N88-BASIC (0x6000-0x7fff) |                        |
| 0x0c000  | 8KB  | N88-BASIC EROM0           | N88_0.ROM, N88EXT0.ROM |
| 0x0e000  | 8KB  | N88-BASIC EROM1           | N88_1.ROM, N88EXT1.ROM |
| 0x10000  | 8KB  | N88-BASIC EROM2           | N88_2.ROM, N88EXT2.ROM |
| 0x12000  | 8KB  | N88-BASIC EROM3           | N88_3.ROM, N88EXT3.ROM |
| 0x14000  | 8KB  | SubSystem ROM             | DISK.ROM               |
| 0x16000  | 24KB | N-BASIC (0x0000-0x5fff)   | N80.ROM, N88N.ROM      |

### `KANJI1.ROM` / `N88KNJ1.ROM` : JIS 第一水準 Kanji ROM (128KB)

### `KANJI2.ROM` / `N88KNJ2.ROM` : JIS 第二水準 Kanji ROM (128KB)

## Optional ROMs

### `JISYO.ROM` / `N88JISHO.ROM` : 辞書ROM / Jisyo ROM (512KB)

PC-8801MH+, 512KB

### `CDBIOS.ROM` : CD BIOS (64KB)

PC-8801MC, 64KB

### `FONT.ROM` : Font (2KB)

If not available, parts of "KANJI1.ROM" (offset 0x1000, size 0x800) is used.

### `N80_2.ROM` : N80 ROM (32KB)

### `N80_3.ROM` : N80V2 ROM (40KB)

### `FONT80SR.ROM` : N80SR font (16KB)

### `E1.ROM` - `E7.ROM` : Ext ROM (16KB each)

diskdrv uses E1.ROM.

### `E8.ROM` : N80 Ext ROM (16KB)

### `ym2608_adpcm_rom.bin` : YM2608 Rhythm ROM (8KB)

YM2608B has its internal ROM for rhythm sounds, encoded in ADPCM-A format,
which is different from the one used in YM2608B's ADPCM (-B) encoding.
The ROM cannot be dumped from the real chip via software.
The sampling rate is fixed at ~18.5kHz (= 1/3 of FM sample rate).

| offset  | size  | Description | Alternative  |
|---------|-------|-------------|--------------|
| 0x0000  | 448B  | Bass Drum   | 2608_BD.WAV  |
| 0x01c0  | 640B  | Snare Drum  | 2608_SD.WAV  |
| 0x0440  | 5952B | Top Cymbal  | 2608_TOP.WAV |
| 0x1b80  | 384B  | Hi-Hat      | 2608_HH.WAV  |
| 0x1d00  | 640B  | Tom Tom     | 2608_TOM.WAV |
| 0x1f80  | 128B  | Rimshot     | 2608_RIM.WAV |
