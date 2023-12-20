// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  Direct3D12 による画面描画
// ---------------------------------------------------------------------------

#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include <memory>
#include <vector>

#include "common/scoped_comptr.h"
#include "win32/screen/windraw.h"

class WinDrawD3D12 : public WinDrawSub {
 public:
  WinDrawD3D12();
  ~WinDrawD3D12() override;

  struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 uv;
  };

  bool Init(HWND hwnd, uint32_t width, uint32_t height, GUID*) override;
  bool Resize(uint32_t width, uint32_t height) override;
  bool CleanUp() override { return true; }
  void SetPalette(Draw::Palette* pe, int index, int nentries) override;
  void SetGUIMode(bool fullscreen) override;
  void DrawScreen(const RECT& rect, bool refresh) override;
  RECT GetFullScreenRect() override;

  bool Lock(uint8_t** image, int* bpl) override;
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
  void SetUpViewPort();

  D3D12_CPU_DESCRIPTOR_HANDLE PrepareCommandList();
  void CommitCommandList();

  bool SetUpTexture();
  bool SetUpVerticesForTexture();
  void PrepareVerticesForTexture();
  bool SetUpIndicesForTexture();
  void PrepareIndicesForTexture();
  bool SetUpPipelineForTexture(D3D12_INPUT_ELEMENT_DESC* input_layout, int num_elements);
  bool SetUpTexturePipeline();

  bool ClearScreen();
  bool DrawTexture(const RECT& rect);
  bool RenderTexture(const RECT& rect);

  void WaitForGPU();

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

  // Texture buffers
  scoped_comptr<ID3D12Resource> upload_buffer_;
  scoped_comptr<ID3D12Resource> texture_buffer_;

  scoped_comptr<ID3D12Fence> fence_;
  uint64_t fence_val_ = 0;

  struct TexRGBA {
    uint8_t R, G, B, A;
  };
  using Palette = Draw::Palette;
  scoped_comptr<ID3D12Resource> texture_;
  scoped_comptr<ID3D12Resource> vert_buffer_;
  scoped_comptr<ID3D12Resource> index_buffer_;
  scoped_comptr<ID3D12DescriptorHeap> tex_desc_heap_;

  // texture data buffer for rendering
  std::vector<TexRGBA> texturedata_;

  bool update_palette_ = false;
  // parent
  HWND hwnd_ = nullptr;
  // child
  HWND hcwnd_ = nullptr;

  uint32_t width_ = 640;
  uint32_t height_ = 400;

  Palette pal_[256]{};
  std::unique_ptr<uint8_t[]> image_;
  int bpl_ = 0;
};
