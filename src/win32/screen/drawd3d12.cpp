// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  Direct3D による画面描画
// ---------------------------------------------------------------------------

#include "win32/screen/drawd3d12.h"

#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace {

#ifndef COMPILE_SHADER_FROM_FILE
#include "win32/screen/vertex_shader.h"
#include "win32/screen/pixel_shader.h"
#endif

constexpr int kTextureWidth = 640;
constexpr int kTextureHeight = 400;
}  // namespace

WinDrawD3D12::WinDrawD3D12() {
  image_ = std::make_unique<uint8_t[]>(kTextureWidth * kTextureHeight);
  bpl_ = kTextureWidth;
}

WinDrawD3D12::~WinDrawD3D12() {
  if (hcwnd_) {
    ::DestroyWindow(hcwnd_);
    hcwnd_ = nullptr;
  }
}

bool WinDrawD3D12::Init(HWND hwnd, uint32_t width, uint32_t height, GUID*) {
  hwnd_ = hwnd;
  if (!CreateD3D())
    return false;

  // texture size is fixed.
  texturedata_.resize(kTextureWidth * kTextureHeight);

  Resize(width, height);
  return true;
}

bool WinDrawD3D12::Resize(uint32_t screen_width, uint32_t screen_height) {
  width_ = screen_width;
  height_ = screen_height;

  status |= static_cast<uint32_t>(Draw::Status::kShouldRefresh);
  ::SetWindowPos(hcwnd_, nullptr, 0, 0, screen_width, screen_height, SWP_SHOWWINDOW);
  return true;
}

void WinDrawD3D12::SetPalette(PALETTEENTRY* pe, int index, int nentries) {
  for (; nentries > 0; nentries--) {
    pal_[index].red = pe->peRed;
    pal_[index].green = pe->peGreen;
    pal_[index].blue = pe->peBlue;
    index++;
    pe++;
  }
  update_palette_ = true;
}

void WinDrawD3D12::SetGUIMode(bool fullscreen) {
  // TODO: full screen support
  if (fullscreen) {
    // Fullscreen
  } else {
    // Windowed
  }
}

void WinDrawD3D12::DrawScreen(const RECT& rect, bool refresh) {
  if (::IsWindow(hwnd_) == FALSE)
    return;

  RECT rc = rect;
  if (refresh || update_palette_) {
    ::SetRect(&rc, 0, 0, kTextureWidth, kTextureHeight);
    update_palette_ = false;
  } else {
    if (::IsRectEmpty(&rc))
      return;
  }
  DrawTexture();
}

RECT WinDrawD3D12::GetFullScreenRect() {
  scoped_comptr<IDXGIOutput> output;
  swap_chain_->GetContainingOutput(&output);
  DXGI_OUTPUT_DESC desc;
  output->GetDesc(&desc);
  return desc.DesktopCoordinates;
}

bool WinDrawD3D12::Lock(uint8_t** image, int* bpl) {
  *image = image_.get();
  *bpl = bpl_;
  return image_ != nullptr;
}

bool WinDrawD3D12::Unlock() {
  status &= ~static_cast<uint32_t>(Draw::Status::kShouldRefresh);
  return true;
}

bool WinDrawD3D12::CreateD3D() {
  CreateBaseWindow();
  if (!hcwnd_)
    return false;

#if defined(_DEBUG)
  {
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug_));
    if (SUCCEEDED(hr)) {
      debug_->EnableDebugLayer();
    }
  }
#endif

  if (!CreateD3D12Device() || !CreateCommandList() || !CreateSwapChain() ||
      !CreateRenderTargetView()) {
    return false;
  }

  if (!root_signature_)
    SetUpRootSignature();

  SetUpTexturePipeline();
  ClearScreen();
  return true;
}

void WinDrawD3D12::CreateBaseWindow() {
  if (!hcwnd_) {
    // Base windowを生成
    hcwnd_ = ::CreateWindowEx(WS_EX_TRANSPARENT, "M88p2 WinUI", "", WS_CHILD, 0, 0, 640, 480, hwnd_,
                              nullptr, nullptr, nullptr);
    if (!hcwnd_)
      return;
  }
  ::ShowWindow(hcwnd_, SW_SHOW);
}

bool WinDrawD3D12::CreateD3D12Device() {
  if (!dxgi_factory_) {
#if defined(_DEBUG)
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory_));
#else
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory_));
#endif
    if (FAILED(hr))
      return false;
  }

  std::vector<IDXGIAdapter*> adapters;
  IDXGIAdapter* tmp_adapter = nullptr;
  for (int i = 0; dxgi_factory_->EnumAdapters(i, &tmp_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
    adapters.push_back(tmp_adapter);
  }

  // Prefer NVIDIA GPU if present.
  for (const auto adapter : adapters) {
    DXGI_ADAPTER_DESC desc = {};
    adapter->GetDesc(&desc);
    std::wstring str_desc = desc.Description;
    if (str_desc.find(L"NVIDIA") != std::string::npos) {
      tmp_adapter = adapter;
      break;
    }
  }

  HRESULT hr = D3D12CreateDevice(tmp_adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&dev_));
  if (FAILED(hr))
    return false;

  if (!fence_) {
    hr = dev_->CreateFence(fence_val_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    return SUCCEEDED(hr);
  }
  return true;
}

bool WinDrawD3D12::CreateCommandList() {
  HRESULT hr =
      dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocator_));
  if (FAILED(hr))
    return false;

  hr = dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocator_.get(), nullptr,
                               IID_PPV_ARGS(&cmd_list_));
  if (FAILED(hr))
    return false;

  D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
  cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  cmd_queue_desc.NodeMask = 0;
  cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  hr = dev_->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue_));
  return SUCCEEDED(hr);
}

bool WinDrawD3D12::CreateSwapChain() {
  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
  swap_chain_desc.Width = width_;
  swap_chain_desc.Height = height_;
  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.Stereo = FALSE;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;
  swap_chain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  HRESULT hr =
      dxgi_factory_->CreateSwapChainForHwnd(cmd_queue_.get(), hcwnd_, &swap_chain_desc, nullptr,
                                            nullptr, (IDXGISwapChain1**)&swap_chain_);
  return SUCCEEDED(hr);
}

bool WinDrawD3D12::CreateRenderTargetView() {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap_desc.NodeMask = 0;
  heap_desc.NumDescriptors = 2;
  heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  HRESULT hr = dev_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap_));
  if (FAILED(hr))
    return false;

  DXGI_SWAP_CHAIN_DESC swc_desc = {};
  hr = swap_chain_->GetDesc(&swc_desc);
  if (FAILED(hr))
    return false;

  back_buffers_.resize(swc_desc.BufferCount);
  for (int i = 0; i < swc_desc.BufferCount; ++i) {
    hr = swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffers_[i]));
    if (FAILED(hr))
      return false;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * i;
    dev_->CreateRenderTargetView(back_buffers_[i], nullptr, handle);
  }
  return true;
}

bool WinDrawD3D12::SetUpShaders() {
  scoped_comptr<ID3DBlob> error_blob;

  if (!vs_blob_) {
#ifdef COMPILE_SHADER_FROM_FILE
    uint32_t flags = 0;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr =
        D3DCompileFromFile(L"vertex_shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                           "BasicVS", "vs_5_0", flags, 0, &vs_blob_, &error_blob);
    if (FAILED(hr)) {
      if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        OutputDebugStringA("The shader file cannot be found.\n");
      } else {
        OutputDebugStringA("Failed to compile the vertex shader.\n");
        std::string errstr;
        errstr.resize(error_blob->GetBufferSize());

        std::copy_n((char*)error_blob->GetBufferPointer(), error_blob->GetBufferSize(),
                    errstr.begin());
        errstr += "\n";

        OutputDebugStringA(errstr.c_str());
      }
    }
#endif
    HRESULT hr = D3DCreateBlob(sizeof(g_BasicVS), &vs_blob_);
    std::copy_n(g_BasicVS, sizeof(g_BasicVS), (char*)vs_blob_->GetBufferPointer());
    if (FAILED(hr))
      return false;
  }

  if (!ps_blob_) {
#ifdef COMPILE_SHADER_FROM_FILE
    uint32_t flags = 0;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr =
        D3DCompileFromFile(L"pixel_shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                           "BasicPS", "ps_5_0", flags, 0, &ps_blob_, &error_blob);
    if (FAILED(hr)) {
      if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        OutputDebugStringA("The shader file cannot be found.\n");
      } else {
        OutputDebugStringA("Failed to compile the pixel shader.\n");
        std::string errstr;
        errstr.resize(error_blob->GetBufferSize());

        std::copy_n((char*)error_blob->GetBufferPointer(), error_blob->GetBufferSize(),
                    errstr.begin());
        errstr += "\n";

        OutputDebugStringA(errstr.c_str());
      }
    }
#endif
    HRESULT hr = D3DCreateBlob(sizeof(g_BasicPS), &ps_blob_);
    std::copy_n(g_BasicPS, sizeof(g_BasicPS), (char*)ps_blob_->GetBufferPointer());
    if (FAILED(hr))
      return false;
  }

  return true;
}

bool WinDrawD3D12::SetUpRootSignature() {
  D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
  root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  D3D12_DESCRIPTOR_RANGE desc_range = {};
  desc_range.NumDescriptors = 1;
  desc_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  desc_range.BaseShaderRegister = 0;
  desc_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  D3D12_ROOT_PARAMETER root_param = {};
  root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  root_param.DescriptorTable.pDescriptorRanges = &desc_range;
  root_param.DescriptorTable.NumDescriptorRanges = 1;

  root_signature_desc.pParameters = &root_param;
  root_signature_desc.NumParameters = 1;

  D3D12_STATIC_SAMPLER_DESC sampler_desc = {};
  sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler_desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  // No interpolation
  sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  // sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
  sampler_desc.MinLOD = 0.0f;
  sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

  root_signature_desc.pStaticSamplers = &sampler_desc;
  root_signature_desc.NumStaticSamplers = 1;

  scoped_comptr<ID3DBlob> root_sig_blob;
  scoped_comptr<ID3DBlob> error_blob;
  HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                           &root_sig_blob, &error_blob);
  if (FAILED(hr)) {
    OutputDebugStringA("Failed to get root signature blob.\n");
    std::string errstr;
    errstr.resize(error_blob->GetBufferSize());

    std::copy_n((char*)error_blob->GetBufferPointer(), error_blob->GetBufferSize(), errstr.begin());
    errstr += "\n";

    OutputDebugStringA(errstr.c_str());
  }

  hr = dev_->CreateRootSignature(0, root_sig_blob->GetBufferPointer(),
                                 root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
  return SUCCEEDED(hr);
}

void WinDrawD3D12::SetUpViewPort() {
  D3D12_VIEWPORT viewport = {};
  viewport.Width = kTextureWidth;
  viewport.Height = kTextureHeight;
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.MaxDepth = 1.0f;
  viewport.MinDepth = 0.0f;

  D3D12_RECT scissor_rect = {};
  scissor_rect.top = 0;
  scissor_rect.left = 0;
  scissor_rect.right = kTextureWidth;
  scissor_rect.bottom = kTextureHeight;

  cmd_list_->RSSetViewports(1, &viewport);
  cmd_list_->RSSetScissorRects(1, &scissor_rect);
}

D3D12_CPU_DESCRIPTOR_HANDLE WinDrawD3D12::PrepareCommandList() {
  auto back_buffer_index = swap_chain_->GetCurrentBackBufferIndex();

  D3D12_RESOURCE_BARRIER barrier_desc = {};
  barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier_desc.Transition.pResource = back_buffers_[back_buffer_index];
  barrier_desc.Transition.Subresource = 0;
  barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  cmd_list_->ResourceBarrier(1, &barrier_desc);

  auto rtv_h = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
  rtv_h.ptr +=
      dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * back_buffer_index;
  cmd_list_->OMSetRenderTargets(1, &rtv_h, true, nullptr);

  return rtv_h;
}

void WinDrawD3D12::CommitCommandList() {
  D3D12_RESOURCE_BARRIER barrier_desc = {};
  barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier_desc.Transition.pResource = back_buffers_[swap_chain_->GetCurrentBackBufferIndex()];
  barrier_desc.Transition.Subresource = 0;
  barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  cmd_list_->ResourceBarrier(1, &barrier_desc);
  cmd_list_->Close();

  ID3D12CommandList* cmd_list[] = {cmd_list_.get()};

  cmd_queue_->ExecuteCommandLists(1, cmd_list);

  cmd_queue_->Signal(fence_.get(), ++fence_val_);
  if (fence_->GetCompletedValue() != fence_val_) {
    auto event = ::CreateEvent(nullptr, false, false, nullptr);
    fence_->SetEventOnCompletion(fence_val_, event);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);
  }

  cmd_allocator_->Reset();
  cmd_list_->Reset(cmd_allocator_.get(), nullptr);

  swap_chain_->Present(1, 0);
}

bool WinDrawD3D12::SetUpTexture() {
  if (!texture_) {
    D3D12_HEAP_PROPERTIES heap_prop_tex = {};
    heap_prop_tex.Type = D3D12_HEAP_TYPE_CUSTOM;
    heap_prop_tex.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heap_prop_tex.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    heap_prop_tex.CreationNodeMask = 0;
    heap_prop_tex.VisibleNodeMask = 0;

    D3D12_RESOURCE_DESC resource_desc_tex = {};
    resource_desc_tex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resource_desc_tex.Width = kTextureWidth;    // fixed
    resource_desc_tex.Height = kTextureHeight;  // fixed
    resource_desc_tex.DepthOrArraySize = 1;
    resource_desc_tex.SampleDesc.Count = 1;
    resource_desc_tex.SampleDesc.Quality = 0;
    resource_desc_tex.MipLevels = 1;
    resource_desc_tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc_tex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc_tex.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = dev_->CreateCommittedResource(
        &heap_prop_tex, D3D12_HEAP_FLAG_NONE, &resource_desc_tex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture_));
    if (FAILED(hr))
      return false;
  }

  if (!tex_desc_heap_) {
    D3D12_DESCRIPTOR_HEAP_DESC tex_desc_heap_desc = {};
    tex_desc_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    tex_desc_heap_desc.NodeMask = 0;
    tex_desc_heap_desc.NumDescriptors = 1;
    tex_desc_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    HRESULT hr = dev_->CreateDescriptorHeap(&tex_desc_heap_desc, IID_PPV_ARGS(&tex_desc_heap_));
    if (FAILED(hr))
      return false;
  }

  return true;
}

namespace {
// clang-format off
constexpr WinDrawD3D12::Vertex vertices[] = {
  {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},  // left bottom
  {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},  // left top
  {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},  // right bottom
  {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}}   // right top
};

constexpr uint16_t indices[] = {
  0, 1, 2, 3
};
// clang-format on
}  // namespace

bool WinDrawD3D12::SetUpVerticesForTexture() {
  if (vert_buffer_)
    return true;

  D3D12_HEAP_PROPERTIES heap_prop = {};
  heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
  heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC resource_desc = {};
  resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource_desc.Width = sizeof(vertices);
  resource_desc.Height = 1;
  resource_desc.DepthOrArraySize = 1;
  resource_desc.MipLevels = 1;
  resource_desc.Format = DXGI_FORMAT_UNKNOWN;
  resource_desc.SampleDesc.Count = 1;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  HRESULT hr = dev_->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&vert_buffer_));
  if (FAILED(hr))
    return false;

  Vertex* vert_map = nullptr;
  hr = vert_buffer_->Map(0, nullptr, (void**)&vert_map);
  if (FAILED(hr))
    return false;

  std::copy(std::begin(vertices), std::end(vertices), vert_map);
  vert_buffer_->Unmap(0, nullptr);

  return true;
}

void WinDrawD3D12::PrepareVerticesForTexture() {
  // rectangle vertex buffer view
  D3D12_VERTEX_BUFFER_VIEW vb_view = {};
  vb_view.BufferLocation = vert_buffer_->GetGPUVirtualAddress();
  vb_view.SizeInBytes = sizeof(vertices);
  vb_view.StrideInBytes = sizeof(Vertex);
  cmd_list_->IASetVertexBuffers(0, 1, &vb_view);
}

bool WinDrawD3D12::SetUpIndicesForTexture() {
  if (index_buffer_)
    return true;

  // create buffer for rectangle indices
  D3D12_HEAP_PROPERTIES heap_prop = {};
  heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
  heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC resource_desc = {};
  resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource_desc.Width = sizeof(indices);
  resource_desc.Height = 1;
  resource_desc.DepthOrArraySize = 1;
  resource_desc.MipLevels = 1;
  resource_desc.Format = DXGI_FORMAT_UNKNOWN;
  resource_desc.SampleDesc.Count = 1;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  HRESULT hr = dev_->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&index_buffer_));
  if (FAILED(hr))
    return false;

  uint16_t* mapped_idx = nullptr;
  index_buffer_->Map(0, nullptr, (void**)&mapped_idx);
  std::copy(std::begin(indices), std::end(indices), mapped_idx);
  index_buffer_->Unmap(0, nullptr);

  return true;
}

void WinDrawD3D12::PrepareIndicesForTexture() {
  D3D12_INDEX_BUFFER_VIEW ib_view = {};
  ib_view.BufferLocation = index_buffer_->GetGPUVirtualAddress();
  ib_view.Format = DXGI_FORMAT_R16_UINT;
  ib_view.SizeInBytes = sizeof(indices);
  cmd_list_->IASetIndexBuffer(&ib_view);
}

bool WinDrawD3D12::SetUpPipelineForTexture(D3D12_INPUT_ELEMENT_DESC* input_layout,
                                           int num_elements) {
  assert(root_signature_);
  assert(vs_blob_);
  assert(ps_blob_);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
  gpipeline.pRootSignature = root_signature_.get();
  gpipeline.VS.pShaderBytecode = vs_blob_->GetBufferPointer();
  gpipeline.VS.BytecodeLength = vs_blob_->GetBufferSize();
  gpipeline.PS.pShaderBytecode = ps_blob_->GetBufferPointer();
  gpipeline.PS.BytecodeLength = ps_blob_->GetBufferSize();

  gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  gpipeline.RasterizerState.MultisampleEnable = false;
  gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  gpipeline.RasterizerState.DepthClipEnable = true;

  gpipeline.BlendState.AlphaToCoverageEnable = false;
  gpipeline.BlendState.IndependentBlendEnable = false;

  D3D12_RENDER_TARGET_BLEND_DESC rendar_target_blend_desc = {};
  rendar_target_blend_desc.BlendEnable = false;
  rendar_target_blend_desc.LogicOpEnable = false;
  rendar_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  gpipeline.BlendState.RenderTarget[0] = rendar_target_blend_desc;

  gpipeline.InputLayout.pInputElementDescs = input_layout;
  gpipeline.InputLayout.NumElements = num_elements;

  gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  gpipeline.NumRenderTargets = 1;
  gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  gpipeline.SampleDesc.Count = 1;
  gpipeline.SampleDesc.Quality = 0;

  HRESULT hr = dev_->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipeline_state_));
  return SUCCEEDED(hr);
}

bool WinDrawD3D12::SetUpTexturePipeline() {
  if (!SetUpVerticesForTexture() || !SetUpIndicesForTexture())
    return false;

  if (!SetUpShaders())
    return false;

  static D3D12_INPUT_ELEMENT_DESC input_layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {// uv
       "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

  if (!pipeline_state_)
    SetUpPipelineForTexture(input_layout, 2);

  return SetUpTexture();
}

bool WinDrawD3D12::ClearScreen() {
  auto rtv_h = PrepareCommandList();
  float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
  cmd_list_->ClearRenderTargetView(rtv_h, clear_color, 0, nullptr);
  CommitCommandList();
  return true;
}

bool WinDrawD3D12::DrawTexture() {
  auto rtv_h = PrepareCommandList();

  PrepareVerticesForTexture();
  PrepareIndicesForTexture();

  cmd_list_->SetPipelineState(pipeline_state_.get());
  cmd_list_->SetGraphicsRootSignature(root_signature_.get());

  if (!RenderTexture())
    return false;

  SetUpViewPort();

  // Using index
  cmd_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  cmd_list_->DrawInstanced(4, 1, 0, 0);

  CommitCommandList();
  return true;
}

bool WinDrawD3D12::RenderTexture() {
  for (int y = 0; y < kTextureHeight; ++y) {
    for (int x = 0; x < kTextureWidth; ++x) {
      auto& rgba = texturedata_[y * kTextureWidth + x];
      uint8_t col = image_[y * bpl_ + x];
      Palette pal = pal_[col];
      rgba.R = pal.red;
      rgba.G = pal.green;
      rgba.B = pal.blue;
      rgba.A = 255;
    }
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;

  dev_->CreateShaderResourceView(texture_.get(), &srv_desc,
                                 tex_desc_heap_->GetCPUDescriptorHandleForHeapStart());

  cmd_list_->SetDescriptorHeaps(1, &tex_desc_heap_);
  cmd_list_->SetGraphicsRootDescriptorTable(0,
                                            tex_desc_heap_->GetGPUDescriptorHandleForHeapStart());

  HRESULT hr =
      texture_->WriteToSubresource(0, nullptr, texturedata_.data(), kTextureWidth * sizeof(TexRGBA),
                                   texturedata_.size() * sizeof(TexRGBA));
  return SUCCEEDED(hr);
}
