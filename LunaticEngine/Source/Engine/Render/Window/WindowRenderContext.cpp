#include "PCH/LunaticPCH.h"
#include "Render/Window/WindowRenderContext.h"

#define SAFE_RELEASE(Obj) if (Obj) { Obj->Release(); Obj = nullptr; }

bool FWindowRenderContext::Create(HWND InWindowHandle, ID3D11Device *InDevice, ID3D11DeviceContext *InDeviceContext)
{
    WindowHandle = InWindowHandle;
    Device = InDevice;
    DeviceContext = InDeviceContext;
    if (!WindowHandle || !Device || !DeviceContext)
    {
        return false;
    }

    CreateSwapChain();
    CreateFrameResources();
    return IsValid();
}

void FWindowRenderContext::Release()
{
    ReleaseFrameResources();
    SAFE_RELEASE(SwapChain);
    WindowHandle = nullptr;
    Device = nullptr;
    DeviceContext = nullptr;
}

void FWindowRenderContext::BeginFrame()
{
    if (!IsValid() || !DeviceContext)
    {
        return;
    }

    DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
}

void FWindowRenderContext::Present()
{
    if (!SwapChain)
    {
        return;
    }

    BOOL bFullscreen = FALSE;
    SwapChain->GetFullscreenState(&bFullscreen, nullptr);
    const UINT PresentFlags = (bTearingSupported && !bFullscreen) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    SwapChain->Present(0, PresentFlags);
}

void FWindowRenderContext::Resize(unsigned int InWidth, unsigned int InHeight)
{
    if (!SwapChain || !DeviceContext || InWidth == 0 || InHeight == 0)
    {
        return;
    }

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseFrameResources();
    SwapChain->ResizeBuffers(0, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, SwapChainFlags);
    CreateFrameResources();
}

void FWindowRenderContext::CreateSwapChain()
{
    IDXGIDevice *DXGIDevice = nullptr;
    IDXGIAdapter *Adapter = nullptr;
    IDXGIFactory *Factory = nullptr;

    if (FAILED(Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice)))
    {
        return;
    }

    if (FAILED(DXGIDevice->GetAdapter(&Adapter)))
    {
        SAFE_RELEASE(DXGIDevice);
        return;
    }

    Adapter->GetParent(__uuidof(IDXGIFactory), (void **)&Factory);

    DXGI_SWAP_CHAIN_DESC Desc = {};
    Desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    Desc.BufferCount = 2;
    Desc.OutputWindow = WindowHandle;
    Desc.Windowed = TRUE;
    Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainFlags = Desc.Flags;
    if (Factory)
    {
        Factory->CreateSwapChain(Device, &Desc, &SwapChain);
    }

    SAFE_RELEASE(Factory);
    SAFE_RELEASE(Adapter);
    SAFE_RELEASE(DXGIDevice);
}

void FWindowRenderContext::CreateFrameResources()
{
    if (!SwapChain || !Device)
    {
        return;
    }

    SAFE_RELEASE(BackBuffer);
    SAFE_RELEASE(RenderTargetView);
    SAFE_RELEASE(DepthStencilBuffer);
    SAFE_RELEASE(DepthStencilView);

    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
    if (!BackBuffer)
    {
        return;
    }

    Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);

    D3D11_TEXTURE2D_DESC BackBufferDesc = {};
    BackBuffer->GetDesc(&BackBufferDesc);

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = BackBufferDesc.Width;
    DepthDesc.Height = BackBufferDesc.Height;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    Device->CreateTexture2D(&DepthDesc, nullptr, &DepthStencilBuffer);
    if (DepthStencilBuffer)
    {
        Device->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);
    }

    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(BackBufferDesc.Width);
    ViewportInfo.Height = static_cast<float>(BackBufferDesc.Height);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;
}

void FWindowRenderContext::ReleaseFrameResources()
{
    SAFE_RELEASE(DepthStencilView);
    SAFE_RELEASE(DepthStencilBuffer);
    SAFE_RELEASE(RenderTargetView);
    SAFE_RELEASE(BackBuffer);
}
