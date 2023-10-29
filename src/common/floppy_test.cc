#include "common/floppy.h"

#include "gtest/gtest.h"

TEST(FloppyTest, TestFloppyInit) {
  FloppyDisk fd;
  fd.Init(FloppyDisk::kMD2D, false);
  EXPECT_EQ(fd.GetNumTracks(), 84);
  EXPECT_EQ(fd.GetType(), FloppyDisk::kMD2D);
  fd.Seek(0);
  EXPECT_EQ(fd.GetSector(), nullptr);

  fd.Init(FloppyDisk::kMD2DD, false);
  EXPECT_EQ(fd.GetNumTracks(), 164);
  EXPECT_EQ(fd.GetType(), FloppyDisk::kMD2DD);
  fd.Seek(0);
  EXPECT_EQ(fd.GetSector(), nullptr);

  fd.Init(FloppyDisk::kMD2HD, false);
  EXPECT_EQ(fd.GetNumTracks(), 164);
  EXPECT_EQ(fd.GetType(), FloppyDisk::kMD2HD);
  fd.Seek(0);
  EXPECT_EQ(fd.GetSector(), nullptr);
}

TEST(FloppyTest, TestFormat) {
  FloppyDisk fd;
  fd.Init(FloppyDisk::kMD2D, false);
  fd.Seek(0);
  fd.FormatTrack(16, 256);
  EXPECT_EQ(fd.GetNumSectors(), 16);
}