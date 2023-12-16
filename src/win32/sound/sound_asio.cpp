// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "win32/sound/sound_asio.h"

#include <mmsystem.h>

// #define LOGNAME "sound_asio"
#include "common/diag.h"

#include "asio/common/asiosys.h"
#include "asio/common/asio.h"
#include "asio/host/asiodrivers.h"

#include <memory>

enum {
  // number of input and outputs supported by the host application
  kMaxInputChannels = 4,
  kMaxOutputChannels = 4
};

// internal data storage
struct DriverInfo {
  win32sound::DriverASIO* self;

  // ASIOInit()
  ASIODriverInfo driver_info;

  // ASIOGetChannels()
  long input_channels;
  long output_channels;

  // ASIOGetBufferSize()
  long min_size;
  long max_size;
  long preferred_size;
  long granularity;

  // ASIOGetSampleRate()
  ASIOSampleRate sample_rate;

  // ASIOOutputReady()
  bool post_output;

  // ASIOGetLatencies ()
  long input_latency;
  long output_latency;

  // ASIOCreateBuffers ()
  // becomes number of actual created input buffers
  long input_buffers;
  // becomes number of actual created output buffers
  long output_buffers;
  ASIOBufferInfo buffer_infos[kMaxInputChannels + kMaxOutputChannels];

  // ASIOGetChannelInfo()
  ASIOChannelInfo channel_infos[kMaxInputChannels + kMaxOutputChannels];
  // The above two arrays share the same indexing, as the data in them are linked together

  // Information from ASIOGetSamplePosition()
  // data is converted to double floats for easier use, however 64 bit integer can be used, too
  double nano_seconds;
  double samples;
  double tc_samples;  // time code samples

  // bufferSwitchTimeInfo()
  ASIOTime t_info;             // time info state
  unsigned long sys_ref_time;  // system reference time, when bufferSwitch() was called
};

namespace {

DriverInfo g_asio_driver_info{};
ASIOCallbacks g_asio_callbacks{};

// Collect the informational data of the driver
long InitASIOStaticData(DriverInfo* asioDriverInfo) {
  // Get the number of available channels
  if (ASIOGetChannels(&asioDriverInfo->input_channels, &asioDriverInfo->output_channels) != ASE_OK)
    return -1;

  // Get the usable buffer sizes
  if (ASIOGetBufferSize(&asioDriverInfo->min_size, &asioDriverInfo->max_size,
                        &asioDriverInfo->preferred_size, &asioDriverInfo->granularity) != ASE_OK)
    return -2;

  // Get the currently selected sample rate
  if (ASIOGetSampleRate(&asioDriverInfo->sample_rate) != ASE_OK)
    return -3;

  if (asioDriverInfo->sample_rate <= 0.0 || asioDriverInfo->sample_rate > 96000.0) {
    // Driver does not store it's internal sample rate, so set it to a know one.
    // Usually you should check beforehand, that the selected sample rate is valid
    // with ASIOCanSampleRate().
    if (ASIOSetSampleRate(44100.0) != ASE_OK)
      return -5;
    if (ASIOGetSampleRate(&asioDriverInfo->sample_rate) != ASE_OK)
      return -6;
  }

  // Check whether the driver requires the ASIOOutputReady() optimization
  // (can be used by the driver to reduce output latency by one block)
  asioDriverInfo->post_output = (ASIOOutputReady() == ASE_OK);
  return 0;
}

//----------------------------------------------------------------------------------
// conversion from 64 bit ASIOSample/ASIOTimeStamp to double float
#if NATIVE_INT64
#define ASIO64toDouble(a) (a)
#else
const double twoRaisedTo32 = 4294967296.;
#define ASIO64toDouble(a) ((a).lo + (a).hi * twoRaisedTo32)
#endif

unsigned long GetSysReferenceTime() {
  // get the system reference time
#if WINDOWS
  return timeGetTime();
#elif MAC
  static const double twoRaisedTo32 = 4294967296.;
  UnsignedWide ys;
  Microseconds(&ys);
  double r = ((double)ys.hi * twoRaisedTo32 + (double)ys.lo);
  return (unsigned long)(r / 1000.);
#endif
}

// the actual processing callback.
ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool /*processNow*/) {
  auto* self = g_asio_driver_info.self;
  return self->BufferSwitchTimeInfo(timeInfo, index);
}

// the actual processing callback.
void bufferSwitch(long index, ASIOBool /*processNow*/) {
  auto* self = g_asio_driver_info.self;
  self->BufferSwitch(index);
}

void sampleRateChanged(ASIOSampleRate sRate) {
  auto* self = g_asio_driver_info.self;
  self->SampleRateChanged(sRate);
}

long asioMessages(long selector, long value, void* message, double* opt) {
  auto* self = g_asio_driver_info.self;
  return self->AsioMessage(selector, value, message, opt);
}

ASIOError CreateASIOBuffers(DriverInfo* asioDriverInfo) {
  // Create buffers for all inputs and outputs of the card with the
  // preferredSize from ASIOGetBufferSize() as buffer size
  long i = 0;
  ASIOError result = ASE_OK;

  // fill the bufferInfos from the start without a gap
  ASIOBufferInfo* info = asioDriverInfo->buffer_infos;

  // prepare inputs (Though this is not necessaily required, no opened inputs will work, too
  if (asioDriverInfo->input_channels > kMaxInputChannels)
    asioDriverInfo->input_buffers = kMaxInputChannels;
  else
    asioDriverInfo->input_buffers = asioDriverInfo->input_channels;
  for (i = 0; i < asioDriverInfo->input_buffers; ++i, ++info) {
    info->isInput = ASIOTrue;
    info->channelNum = i;
    info->buffers[0] = nullptr;
    info->buffers[1] = nullptr;
  }

  // prepare outputs
  if (asioDriverInfo->output_channels > kMaxOutputChannels)
    asioDriverInfo->output_buffers = kMaxOutputChannels;
  else
    asioDriverInfo->output_buffers = asioDriverInfo->output_channels;
  for (i = 0; i < asioDriverInfo->output_buffers; ++i, ++info) {
    info->isInput = ASIOFalse;
    info->channelNum = i;
    info->buffers[0] = nullptr;
    info->buffers[1] = nullptr;
  }

  // create and activate buffers
  result = ASIOCreateBuffers(asioDriverInfo->buffer_infos,
                             asioDriverInfo->input_buffers + asioDriverInfo->output_buffers,
                             asioDriverInfo->preferred_size, &g_asio_callbacks);
  if (result != ASE_OK)
    return result;

  // now get all the buffer details, sample word length, name, word clock group and activation
  for (i = 0; i < asioDriverInfo->input_buffers + asioDriverInfo->output_buffers; ++i) {
    asioDriverInfo->channel_infos[i].channel = asioDriverInfo->buffer_infos[i].channelNum;
    asioDriverInfo->channel_infos[i].isInput = asioDriverInfo->buffer_infos[i].isInput;
    result = ASIOGetChannelInfo(&asioDriverInfo->channel_infos[i]);
    if (result != ASE_OK)
      return result;
  }

  // Get the input and output latencies.
  // Latencies often are only valid after ASIOCreateBuffers()
  // (input latency is the age of the first sample in the currently returned audio block)
  // (output latency is the time the first sample in the currently returned audio block requires
  // to get to the output)
  return ASIOGetLatencies(&asioDriverInfo->input_latency, &asioDriverInfo->output_latency);
}

}  // namespace

//----------------------------------------------------------------------------------
// some external references
extern AsioDrivers* asioDrivers;
bool loadAsioDriver(char* name);

using namespace win32sound;

DriverASIO::DriverASIO() : Driver() {
  playing_ = false;
  mixalways = false;
}

DriverASIO::~DriverASIO() {
  CleanUp();

  ASIOStop();
  ASIODisposeBuffers();
  ASIOExit();
}

bool DriverASIO::Init(SoundSource* source,
                      HWND hwnd,
                      uint32_t rate,
                      uint32_t ch,
                      uint32_t /*buflen_ms*/) {
  if (playing_)
    return false;

  src_ = source;
  sample_shift_ = 1 + (ch == 2 ? 1 : 0);

  if (preferred_driver_.empty() || !loadAsioDriver(const_cast<char*>(preferred_driver_.c_str()))) {
    preferred_driver_.clear();
    FindAvailableDriver();
    preferred_driver_ = current_driver_;
  } else {
    // loadAsioDriver() above succeeded.
    current_driver_ = preferred_driver_;
  }

  if (current_driver_.empty())
    return false;

  g_asio_driver_info.self = this;

  if (ASIOInit(&g_asio_driver_info.driver_info) != ASE_OK)
    return false;

  if (InitASIOStaticData(&g_asio_driver_info) != 0)
    return false;

  sample_rate_ = static_cast<uint32_t>(g_asio_driver_info.sample_rate);

  // you might want to check whether the ASIOControlPanel() can open
  // ASIOControlPanel();

  // set up the asioCallback structure and create the ASIO data buffer
  g_asio_callbacks.bufferSwitch = &bufferSwitch;
  g_asio_callbacks.sampleRateDidChange = &sampleRateChanged;
  g_asio_callbacks.asioMessage = &asioMessages;
  g_asio_callbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;

  if (CreateASIOBuffers(&g_asio_driver_info) != ASE_OK) {
    ASIOExit();
    return false;
  }

  if (ASIOStart() != ASE_OK) {
    ASIODisposeBuffers();
    ASIOExit();
    return false;
  }

  return true;
}

bool DriverASIO::CleanUp() {
  Stop();
  return true;
}

void DriverASIO::FindAvailableDriver() {
  // TODO: factor out this as a static function.
  AsioDrivers ads;
  constexpr int kMaxDrivers = 10;
  char buffer[kMaxDrivers][32] = {};
  char* driver_name[kMaxDrivers];
  for (int i = 0; i < kMaxDrivers; ++i) {
    driver_name[i] = buffer[i];
  }
  long ndrivers = ads.getDriverNames(driver_name, kMaxDrivers);

  available_drivers_.clear();
  for (long i = 0; i < ndrivers; ++i) {
    available_drivers_.emplace_back(driver_name[i]);
  }

  for (auto& driver : available_drivers_) {
    // Pick the first available driver.
    if (loadAsioDriver(const_cast<char*>(driver.c_str()))) {
      current_driver_ = driver;
      break;
    }
  }
}

void DriverASIO::Start() {
  if (!playing_) {
    playing_ = true;
  }
}

void DriverASIO::Stop() {
  if (playing_) {
    playing_ = false;
  }
}

void DriverASIO::BufferSwitch(int index) {
  // Beware that this is normally in a separate thread, hence be sure that you take care
  // about thread synchronization. This is omitted here for simplicity.

  // as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
  // though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the
  // according flags
  ASIOTime timeInfo;
  memset(&timeInfo, 0, sizeof(timeInfo));

  // get the time stamp of the buffer, not necessary if no
  // synchronization to other media is required
  if (ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) ==
      ASE_OK)
    timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

  BufferSwitchTimeInfo(&timeInfo, index /*, processNow*/);
}

ASIOTime* DriverASIO::BufferSwitchTimeInfo(ASIOTime* timeInfo, long index) {
  // Called from a different thread.
  // TODO: get the locking correct.
  // std::lock_guard<std::mutex> lock(mtx_);

  // Store the timeInfo for later use
  g_asio_driver_info.t_info = *timeInfo;

  // Get the time stamp of the buffer, not necessary if no
  // synchronization to other media is required
  if (timeInfo->timeInfo.flags & kSystemTimeValid)
    g_asio_driver_info.nano_seconds = ASIO64toDouble(timeInfo->timeInfo.systemTime);
  else
    g_asio_driver_info.nano_seconds = 0;

  if (timeInfo->timeInfo.flags & kSamplePositionValid)
    g_asio_driver_info.samples = ASIO64toDouble(timeInfo->timeInfo.samplePosition);
  else
    g_asio_driver_info.samples = 0;

  if (timeInfo->timeCode.flags & kTcValid)
    g_asio_driver_info.tc_samples = ASIO64toDouble(timeInfo->timeCode.timeCodeSamples);
  else
    g_asio_driver_info.tc_samples = 0;

  // Get the system reference time
  g_asio_driver_info.sys_ref_time = GetSysReferenceTime();

  // buffer size in samples
  long buffSize = g_asio_driver_info.preferred_size;

  std::unique_ptr<Sample[]> buf = std::make_unique<Sample[]>(buffSize * 2);
  src_->Get(buf.get(), buffSize);

  // perform the processing
  bool first = true;
  for (int i = 0; i < g_asio_driver_info.input_buffers + g_asio_driver_info.output_buffers; ++i) {
    if (g_asio_driver_info.buffer_infos[i].isInput)
      continue;
    // OK do processing for the outputs only
    auto* dest = static_cast<uint8_t*>(g_asio_driver_info.buffer_infos[i].buffers[index]);
    switch (g_asio_driver_info.channel_infos[i].type) {
      case ASIOSTInt16LSB:
        memset(dest, 0, buffSize * 2);
        break;
      case ASIOSTInt24LSB:  // used for 20 bits as well
        memset(dest, 0, buffSize * 3);
        break;
      case ASIOSTInt32LSB: {
        auto* dest32 = reinterpret_cast<int32_t*>(dest);
        Sample* ptr = buf.get() + (first ? 0 : 1);
        first = false;
        for (int j = 0; j < buffSize; ++j) {
          *dest32++ = (int32_t)(*ptr) << 16;
          ptr += 2;
        }
      } break;
      case ASIOSTFloat32LSB:  // IEEE 754 32bit float, as found on Intel x86 architecture
        memset(dest, 0, buffSize * 4);
        break;
      case ASIOSTFloat64LSB:  // IEEE 754 64bit double float, as found on Intel x86 architecture
        memset(dest, 0, buffSize * 8);
        break;

        // these are used for 32bit data buffer, with different alignment of the data inside
        // 32 bit PCI bus systems can more easily used with these
      case ASIOSTInt32LSB16:  // 32bit data with 18bit alignment
      case ASIOSTInt32LSB18:  // 32bit data with 18bit alignment
      case ASIOSTInt32LSB20:  // 32bit data with 20bit alignment
      case ASIOSTInt32LSB24:  // 32bit data with 24bit alignment
        memset(dest, 0, buffSize * 4);
        break;

      case ASIOSTInt16MSB:
        memset(dest, 0, buffSize * 2);
        break;
      case ASIOSTInt24MSB:  // used for 20 bits as well
        memset(dest, 0, buffSize * 3);
        break;
      case ASIOSTInt32MSB:
        memset(dest, 0, buffSize * 4);
        break;
      case ASIOSTFloat32MSB:  // IEEE 754 32 bit float, as found on Intel x86 architecture
        memset(dest, 0, buffSize * 4);
        break;
      case ASIOSTFloat64MSB:  // IEEE 754 64 bit double float, as found on Intel x86 architecture
        memset(dest, 0, buffSize * 8);
        break;

        // these are used for 32bit data buffer, with different alignment of the data inside
        // 32 bit PCI bus systems can more easily used with these
      case ASIOSTInt32MSB16:  // 32bit data with 18bit alignment
      case ASIOSTInt32MSB18:  // 32bit data with 18bit alignment
      case ASIOSTInt32MSB20:  // 32bit data with 20bit alignment
      case ASIOSTInt32MSB24:  // 32bit data with 24bit alignment
        memset(dest, 0, buffSize * 4);
        break;
    }
  }

  // finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in
  // place
  if (g_asio_driver_info.post_output)
    ASIOOutputReady();

  return nullptr;
}

void DriverASIO::SampleRateChanged(double rate) {
  // do whatever you need to do if the sample rate changed
  // usually this only happens during external sync.
  // Audio processing is not stopped by the driver, actual sample rate
  // might not have even changed, maybe only the sample rate status of an
  // AES/EBU or S/PDIF digital input at the audio device.
  // You might have to update time/sample related conversion routines, etc.
}

long DriverASIO::AsioMessage(long selector, long value, void* message, double* opt) {
  // currently the parameters "value", "message" and "opt" are not used.
  long ret = 0;
  switch (selector) {
    case kAsioSelectorSupported:
      if (value == kAsioResetRequest || value == kAsioEngineVersion ||
          value == kAsioResyncRequest ||
          value == kAsioLatenciesChanged
          // the following three were added for ASIO 2.0, you don't necessarily have to support them
          || value == kAsioSupportsTimeInfo || value == kAsioSupportsTimeCode ||
          value == kAsioSupportsInputMonitor)
        ret = 1L;
      break;
    case kAsioResetRequest:
      // defer the task and perform the reset of the driver during the next "safe" situation
      // You cannot reset the driver right now, as this code is called from the driver.
      // Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(),
      // Destruction Afterwards you initialize the driver again. asioDriverInfo.stopped;  // In this
      // sample the processing will just stop
      ret = 1L;
      break;
    case kAsioResyncRequest:
      // This informs the application, that the driver encountered some non fatal data loss.
      // It is used for synchronization purposes of different media.
      // Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
      // Windows Multimedia system, which could loose data because the Mutex was hold too long
      // by another thread.
      // However a driver can issue it in other situations, too.
      ret = 1L;
      break;
    case kAsioLatenciesChanged:
      // This will inform the host application that the drivers were latencies changed.
      // Beware, it this does not mean that the buffer sizes have changed!
      // You might need to update internal delay data.
      ret = 1L;
      break;
    case kAsioEngineVersion:
      // return the supported ASIO version of the host application
      // If a host applications does not implement this selector, ASIO 1.0 is assumed
      // by the driver
      ret = 2L;
      break;
    case kAsioSupportsTimeInfo:
      // informs the driver whether the asioCallbacks.bufferSwitchTimeInfo() callback
      // is supported.
      // For compatibility with ASIO 1.0 drivers the host application should always support
      // the "old" bufferSwitch method, too.
      ret = 1;
      break;
    case kAsioSupportsTimeCode:
      // informs the driver wether application is interested in time code info.
      // If an application does not need to know about time code, the driver has less work
      // to do.
      ret = 0;
      break;
  }
  return ret;
}
