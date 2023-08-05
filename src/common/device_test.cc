

#include "common/device.h"

#include "gtest/gtest.h"

class DeviceListTest : public testing::Test {
 public:
  DeviceListTest() = default;
  ~DeviceListTest() = default;

  void SetUp() override { devlist_ = new DeviceList();
  }

  void TearDown() override {
    delete devlist_;
  }
 private:
  DeviceList* devlist_;
};

TEST(DeviceIdTest, TestId) {
  Device::ID id = 0x123;
  Device x = Device(id);
  EXPECT_EQ(x.GetID(), id);

  Device::ID id2 = 0x1234;
  x.SetID(id2);
  EXPECT_EQ(x.GetID(), id2);
}

TEST(DeviceListTest, SingleDev) {
  DeviceList devlist;
  Device dev1(0x1);
  devlist.Add(&dev1);
  IDevice* x = devlist.Find(0x1);
  EXPECT_EQ(x->GetID(), dev1.GetID());

  devlist.Del(&dev1);
  x = devlist.Find(0x1);
  EXPECT_EQ(x, nullptr);
}

TEST(DeviceListTest, MultiDev) {
  DeviceList devlist;
  Device dev1(0x1);
  devlist.Add(&dev1);
  devlist.Add(&dev1);
  devlist.Add(&dev1);

  IDevice* x = devlist.Find(0x1);
  EXPECT_EQ(x->GetID(), dev1.GetID());

  // Del removes only one instance.
  devlist.Del(&dev1);
  x = devlist.Find(0x1);
  EXPECT_NE(x, nullptr);
  devlist.Del(&dev1);
  x = devlist.Find(0x1);
  EXPECT_NE(x, nullptr);

  // After removing last one, it should be nullptr.
  devlist.Del(&dev1);
  x = devlist.Find(0x1);
  EXPECT_EQ(x, nullptr);
}

TEST(DeviceListTest, MultiDev2) {
  DeviceList devlist;
  Device dev1(0x1);
  Device dev2(0x2);
  Device dev3(0x3);
  devlist.Add(&dev1);
  devlist.Add(&dev2);
  devlist.Add(&dev3);

  IDevice* x = devlist.Find(0x1);
  EXPECT_EQ(x->GetID(), dev1.GetID());
  x = devlist.Find(0x2);
  EXPECT_EQ(x->GetID(), dev2.GetID());
  x = devlist.Find(0x3);
  EXPECT_EQ(x->GetID(), dev3.GetID());

  devlist.Del(&dev1);
  x = devlist.Find(0x1);
  EXPECT_EQ(x, nullptr);

  devlist.Del(&dev2);
  x = devlist.Find(0x2);
  EXPECT_EQ(x, nullptr);

  // After removing last one, it should be nullptr.
  devlist.Del(&dev3);
  x = devlist.Find(0x3);
  EXPECT_EQ(x, nullptr);
}
