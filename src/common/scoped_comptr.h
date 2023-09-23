#pragma once

#include <unknwn.h>

template <class T,  const IID* interface_id = &__uuidof(T)>
class scoped_comptr {
 public:
  scoped_comptr() = default;
  ~scoped_comptr() { reset(); }

  scoped_comptr(const scoped_comptr&) = delete;
  scoped_comptr& operator=(const scoped_comptr&) = delete;

  scoped_comptr(scoped_comptr&& other) noexcept { *this = std::move(other); }
  scoped_comptr& operator=(scoped_comptr&& other) noexcept {
    reset();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  static const IID& iid() {
    return *interface_id;
  }

  T* detach() {
    T* p = ptr_;
    ptr_ = NULL;
    return p;
  }

  void reset() {
    if (ptr_) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }

  [[nodiscard]] T* get() const { return ptr_; }
  T** operator&() { return &ptr_; }
  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  bool operator!() const { return !ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T* ptr_ = nullptr;
};
