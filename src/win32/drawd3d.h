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

  ID3D12Device* dev_ = nullptr;
  IDXGIFactory6* dxgi_factory_ = nullptr;
  IDXGISwapChain4* swap_chain_ = nullptr;

  // DX12 Command
  ID3D12CommandAllocator* cmd_allocator_ = nullptr;
  ID3D12GraphicsCommandList* cmd_list_ = nullptr;
  ID3D12CommandQueue* cmd_queue_ = nullptr;

  ID3D12DescriptorHeap* rtv_heap_ = nullptr;
  std::vector<ID3D12Resource*> back_buffers_;

  // DX12 Pipeline
  ID3D12PipelineState* pipeline_state_ = nullptr;
  ID3D12RootSignature* root_signature_ = nullptr;

  // Shaders
  ID3DBlob* vs_blob_ = nullptr;
  ID3DBlob* ps_blob_ = nullptr;

  struct TexRGBA {
    uint8_t R, G, B, A;
  };
  struct Palette {
    uint8_t r, g, b, a;
  };
  ID3D12Resource* texture_ = nullptr;
  ID3D12Resource* vert_buffer_ = nullptr;
  ID3D12Resource* index_buffer_ = nullptr;
  ID3D12DescriptorHeap* tex_desc_heap_ = nullptr;

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
