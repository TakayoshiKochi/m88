# D88 Image Format

The format was originaly propsed for P88SR and was described at http://www1.plala.or.jp/aoto/tech.htm
but the page is no longer available. You can find the page via the Wayback Machine.

All multi-byte data are stored in little-endian.

## Disk image

| offset | size | Description               |
|--------|------|---------------------------|
| 0      | 688B | Disk header               |
| 0x02b0 | xx   | Track 0 data              |
| xx     | xx   | Track 1 data              |
| ...    | ...  | ...                       |
| xx     | xx   | Track 83 data (2D)        |
| ...    | ...  | ...                       |
| xx     | xx   | Track 163 data (2DD, 2HD) |

### Notes

1. The size of each track is not fixed.
1. The number of tracks vary depending on the disk type. The number of tracks is calculated from the earliest track data offset.
1. A track may or may not exist.  Illegal offset value (e.g. 0, 0xffffffff, values beyond EOF) is treated as 'non-existent'.
1. Some illegal d88 image may share multiple tracks, or overlapping tracks.

## Disk header

| offset | size     | Description                                      |
|--------|----------|--------------------------------------------------|
| 0x0000 | 17B      | Disk name (ASCIIZ)                               |
| 0x0011 | 9B       | Reserved (0 filled)                              |
| 0x001a | 1B       | Write protected flag (00: none, 0x10: protected) |
| 0x001b | 1B       | Media type (0x00: 2D, 0x10: 2DD, 0x20: 2HD)      |
| 0x001c | 4B       | size of the disk                                 |
| 0x0020 | 4B x 164 | Track offset table                               |

### Notes

1. Disk name assumes Shift-JIS encoding.
1. The size of the disk excludes the header size.
1. Track may not be stored in the order of track number.

## Track

Concatenated sectors, no special header section for each track.

## Sector

| offset | size     | Description                                          |
|--------|----------|------------------------------------------------------|
| 0x0000 | 1B       | ID C                                                 |
| 0x0001 | 1B       | ID H                                                 |
| 0x0002 | 1B       | ID R                                                 |
| 0x0003 | 1B       | ID N                                                 |
| 0x0004 | 2B       | Number of sectors                                    |
| 0x0006 | 1B       | Density (0x00: double-density, 0x40: single-density) |
| 0x0007 | 1B       | Deleted Data (0x00: normal, 0x10: deleted data)      |
| 0x0008 | 1B       | Status (0x00: normal, other: Disk BIOS status)       |
| 0x0009 | 5B       | Reserved (0 filled)                                  |
| 0x000e | 2B       | Sector size                                          |
| 0x0010 | variable | Sector data                                          |

### Notes

1. `Number of sectors` should indicate the number of sectors in the track, but it is not always correct.
