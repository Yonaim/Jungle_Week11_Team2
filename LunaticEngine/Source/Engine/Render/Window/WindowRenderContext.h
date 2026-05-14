#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

class FWindowRenderContext
{
  public:
    bool Create(HWND InWindowHandle, ID3D11Device *InDevice, ID3D11DeviceContext *InDeviceContext);
    void Release();

    void BeginFrame();
    void Present();
    void Resize(unsigned int InWidth, unsigned int InHeight);

    ID3D11RenderTargetView *GetRenderTargetView() const { return RenderTargetView; }
    ID3D11DepthStencilView *GetDepthStencilView() const { return DepthStencilView; }
    const D3D11_VIEWPORT &GetViewport() const { return ViewportInfo; }
    bool IsValid() const { return SwapChain && RenderTargetView && DepthStencilView; }

  private:
    void CreateSwapChain();
    void CreateFrameResources();
    void ReleaseFrameResources();

  private:
    HWND WindowHandle = nullptr;
    ID3D11Device *Device = nullptr;
    ID3D11DeviceContext *DeviceContext = nullptr;
    IDXGISwapChain *SwapChain = nullptr;
    ID3D11Texture2D *BackBuffer = nullptr;
    ID3D11RenderTargetView *RenderTargetView = nullptr;
    ID3D11Texture2D *DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView *DepthStencilView = nullptr;
    D3D11_VIEWPORT ViewportInfo = {};
    UINT SwapChainFlags = 0;
    BOOL bTearingSupported = FALSE;
    float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};
