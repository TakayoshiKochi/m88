#pragma once

#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdint.h>
#include <atomic>

template <class T>
class Threadable {
 public:
  Threadable() = default;
  ~Threadable() {
    if (hthread_) {
      TerminateThread(hthread_, 0);
      CloseHandle(hthread_);
      hthread_ = nullptr;
    }
  }

  void StartThread() {
    if (!stopped_)
      return;
    hthread_ = (HANDLE)_beginthreadex(nullptr, 0, ThreadEntry, reinterpret_cast<void*>(this), 0,
                                      &thread_id_);
  }

  // Should be called from the parent thread.
  void RequestThreadStop() {
    stop_request_ = true;
    if (WAIT_TIMEOUT == WaitForSingleObject(hthread_, 3000)) {
      TerminateThread(hthread_, 0);
    }
    assert(stopped_);
    CloseHandle(hthread_);
    hthread_ = nullptr;
  }

 protected:
  bool StopRequested() const { return stop_request_; }
  bool Stopped() const { return stopped_; }

  HANDLE hthread_ = nullptr;

 private:
  uint32_t ThreadMain() {
    stopped_ = false;
    auto& derived = static_cast<T&>(*this);
    derived.ThreadInit();
    while (!StopRequested()) {
      if (!derived.ThreadLoop())
        break;
    }
    stopped_ = true;
    return 0;
  }

  // Windows specific
  static uint32_t CALLBACK ThreadEntry(LPVOID arg);

  std::atomic<bool> stop_request_ = false;
  std::atomic<bool> stopped_ = true;
  uint32_t thread_id_ = 0;
};

// static
template <class T>
uint32_t Threadable<T>::ThreadEntry(LPVOID arg) {
  auto* self = static_cast<T*>(arg);
  return self->ThreadMain();
}
