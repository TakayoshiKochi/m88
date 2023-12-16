// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

template <class T>
class scoped_handle {
 public:
  scoped_handle() = default;
  ~scoped_handle() { reset(); }

  scoped_handle(const scoped_handle&) = delete;
  scoped_handle& operator=(const scoped_handle&) = delete;

  scoped_handle(scoped_handle&& other) noexcept { *this = std::move(other); }
  scoped_handle& operator=(scoped_handle&& other) noexcept {
    reset();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  static const IID& iid() { return *T::interface_id; }

  T detach() {
    T p = ptr_;
    ptr_ = nullptr;
    return p;
  }

  void reset() {
    if (ptr_) {
      CloseHandle(ptr_);
      ptr_ = nullptr;
    }
  }

  void reset(T handle) {
    reset();
    ptr_ = handle;
  }

  [[nodiscard]] T get() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  bool operator!() const { return !ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T ptr_ = nullptr;
};
