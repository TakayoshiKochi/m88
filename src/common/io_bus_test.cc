#include "common/io_bus.h"

#include "gtest/gtest.h"

class MockDevice : public Device {
 public:
  explicit MockDevice(ID id) : Device(id) {}
  ~MockDevice() override = default;

  [[nodiscard]] const Descriptor* GetDesc() const override { return &descriptor; }
  void Reset() {
    port_ = 0;
    data_ = 0;
  }

  uint32_t In(uint32_t port) {
    port_ = port;
    return data_;
  }

  void Out(uint32_t port, uint32_t data) {
    port_ = port;
    data_ = data;
  }

  [[nodiscard]] uint32_t port() const { return port_; }

 private:
  uint32_t port_ = 0;
  uint32_t data_ = 0;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

const Device::Descriptor MockDevice::descriptor = {indef, outdef};

const Device::InFuncPtr MockDevice::indef[] = {
  static_cast<Device::InFuncPtr>(&MockDevice::In)
};

const Device::OutFuncPtr MockDevice::outdef[] = {
  static_cast<Device::OutFuncPtr>(&MockDevice::Out)
};

class IOBusTest : public testing::Test {
 public:
  IOBusTest() : dev1_(1), dev2_(2) {}
  ~IOBusTest() override = default;

  void SetUp() override { bus_.Init(256, nullptr); }
  void TearDown() override { bus_.Init(0, nullptr); }

 protected:
  IOBus bus_;
  MockDevice dev1_;
  MockDevice dev2_;
};

TEST_F(IOBusTest, SingleDevice) {
  constexpr uint32_t kInPort = 0x10;
  constexpr uint32_t kOutPort = 0x11;

  bus_.ConnectIn(kInPort, &dev1_, static_cast<IOBus::InFuncPtr>(&MockDevice::In));
  bus_.ConnectOut(kOutPort, &dev1_, static_cast<IOBus::OutFuncPtr>(&MockDevice::Out));

  // precondition
  EXPECT_EQ(dev1_.port(), 0);

  // test body
  bus_.Out(kOutPort, 0x1234);
  EXPECT_EQ(dev1_.port(), kOutPort);
  EXPECT_EQ(dev1_.In(kInPort), 0x1234);
  EXPECT_EQ(dev1_.port(), kInPort);
}

TEST_F(IOBusTest, DisconnectDevice) {
  constexpr uint32_t kInPort = 0x10;
  constexpr uint32_t kOutPort = 0x11;

  bus_.ConnectIn(kInPort, &dev1_, static_cast<IOBus::InFuncPtr>(&MockDevice::In));
  bus_.ConnectOut(kOutPort, &dev1_, static_cast<IOBus::OutFuncPtr>(&MockDevice::Out));

  // precondition
  EXPECT_EQ(dev1_.port(), 0);

  // test body
  bus_.Out(kOutPort, 0x1234);
  EXPECT_EQ(dev1_.port(), kOutPort);
  EXPECT_EQ(dev1_.In(kInPort), 0x1234);

  bus_.Disconnect(&dev1_);
  EXPECT_EQ(bus_.In(kInPort), 0xff);
}

TEST_F(IOBusTest, MultiDevices) {
  constexpr uint32_t kInPort = 0x10;
  constexpr uint32_t kOutPort = 0x11;

  bus_.ConnectIn(kInPort, &dev1_, static_cast<IOBus::InFuncPtr>(&MockDevice::In));
  bus_.ConnectOut(kOutPort, &dev1_, static_cast<IOBus::OutFuncPtr>(&MockDevice::Out));
  bus_.ConnectIn(kInPort, &dev2_, static_cast<IOBus::InFuncPtr>(&MockDevice::In));
  bus_.ConnectOut(kOutPort, &dev2_, static_cast<IOBus::OutFuncPtr>(&MockDevice::Out));

  // precondition
  EXPECT_EQ(dev1_.port(), 0);
  EXPECT_EQ(dev2_.port(), 0);

  // test body
  bus_.Out(kOutPort, 0x1234);
  EXPECT_EQ(dev1_.port(), kOutPort);
  EXPECT_EQ(dev1_.In(kInPort), 0x1234);
  EXPECT_EQ(dev2_.port(), kOutPort);
  EXPECT_EQ(dev2_.In(kInPort), 0x1234);
}

TEST_F(IOBusTest, DummyDevice) {
  // Out to non-existent port should not cause any error.
  bus_.Out(0, 0);
  // In from non-existent port should return 0xff.
  EXPECT_EQ(bus_.In(0), 0xff);
}

TEST_F(IOBusTest, ConnectorTest) {
  constexpr uint32_t kInPort = 0x10;
  constexpr uint32_t kOutPort = 0x11;

  // dev1 and dev2 will be connected to different ports.
  static const IOBus::Connector c_mock1[] = {
      {kOutPort, IOBus::portout, 0}, {kInPort, IOBus::portin, 0}, {0, 0, 0}};
  static const IOBus::Connector c_mock2[] = {
      {kOutPort + 2, IOBus::portout | IOBus::sync, 0}, {kInPort + 2, IOBus::portin | IOBus::sync, 0}, {0, 0, 0}};

  bus_.Connect(&dev1_, c_mock1);
  bus_.Connect(&dev2_, c_mock2);

  // precondition
  EXPECT_EQ(dev1_.port(), 0);
  EXPECT_EQ(dev2_.port(), 0);

  // IOBus::sync will be reflected to IsSyncPort().
  EXPECT_FALSE(bus_.IsSyncPort(kOutPort));
  EXPECT_FALSE(bus_.IsSyncPort(kInPort));
  EXPECT_TRUE(bus_.IsSyncPort(kOutPort + 2));
  EXPECT_TRUE(bus_.IsSyncPort(kInPort + 2));

  // test body
  bus_.Out(kOutPort, 0x1234);
  bus_.Out(kOutPort + 2, 0x2345);
  EXPECT_EQ(dev1_.port(), kOutPort);
  EXPECT_EQ(dev1_.In(kInPort), 0x1234);
  EXPECT_EQ(dev2_.port(), kOutPort + 2);
  EXPECT_EQ(dev2_.In(kInPort + 2), 0x2345);
}