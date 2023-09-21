// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  Direct2D による画面描画
// ---------------------------------------------------------------------------

#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include <vector>

#include "win32/windraw.h"

template <class T>
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

class WinDrawD3D : public WinDrawSub {
 public:
  WinDrawD3D();
  ~WinDrawD3D() override;

  bool Init(HWND hwnd, uint32_t w, uint32_t h, GUID*) override;
  bool Resize(uint32_t width, uint32_t height) override;
  bool CleanUp() override;
  void SetPalette(PALETTEENTRY* pe, int index, int nentries) override;
  void SetGUIMode(bool guimode) override;
  void DrawScreen(const RECT& rect, bool refresh) override;

  bool Lock(uint8_t** pimage, int* pbpl) override;
  bool Unlock() override;

 private:
  bool CreateD3D();

  void CreateBaseWindow();
  bool CreateD3D12Device();
  bool CreateCommandList();
  bool CreateSwapChain();
  bool CreateRenderTargetView();

  bool SetUpShaders();
  bool SetUpRootSignature();
  bool SetUpPipeline(D3D12_INPUT_ELEMENT_DESC* input_layout, int num_elements);
  void SetUpViewPort();

  D3D12_CPU_DESCRIPTOR_HANDLE PrepareCommandList();
  void CommitCommandList();

  bool SetUpTexture();

  bool ClearScreen();
  bool DrawTexture();

  scoped_comptr<ID3D12Device> dev_;
  scoped_comptr<IDXGIFactory6> dxgi_factory_;
  scoped_comptr<IDXGISwapChain4> swap_chain_;

  // DX12 Command
  scoped_comptr<ID3D12CommandAllocator> cmd_allocator_;
  scoped_comptr<ID3D12GraphicsCommandList> cmd_list_;
  scoped_comptr<ID3D12CommandQueue> cmd_queue_;

  scoped_comptr<ID3D12DescriptorHeap> rtv_heap_;
  std::vector<ID3D12Resource*> back_buffers_;

  // DX12 Pipeline
  scoped_comptr<ID3D12PipelineState> pipeline_state_;
  scoped_comptr<ID3D12RootSignature> root_signature_;

  // Shaders
  scoped_comptr<ID3DBlob> vs_blob_;
  scoped_comptr<ID3DBlob> ps_blob_;

  struct TexRGBA {
    uint8_t R, G, B, A;
  };
  struct Palette {
    uint8_t r, g, b, a;
  };
  scoped_comptr<ID3D12Resource> texture_;
  scoped_comptr<ID3D12Resource> vert_buffer_;
  scoped_comptr<ID3D12Resource> index_buffer_;
  scoped_comptr<ID3D12DescriptorHeap> tex_desc_heap_;

  bool update_palette_ = false;
  // parent
  HWND hwnd_ = nullptr;
  // child
  HWND hcwnd_ = nullptr;

  uint32_t width_ = 640;
  uint32_t height_ = 400;

  Palette pal_[256]{};
  uint8_t* image_ = nullptr;
  int bpl_ = 0;
};
