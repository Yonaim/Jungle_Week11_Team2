#pragma once

#include <cstdint>

class FWindowsWindow;
class FD3DDevice;
struct ImGuiContext;
struct ImFont;

/**
 * Editor 전체에서 사용하는 단일 ImGui Context / Backend 관리자.
 *
 * 중요한 규칙:
 * - ImGui::CreateContext(), ImGui_ImplWin32_Init(), ImGui_ImplDX11_Init()은 여기에서 한 번만 수행한다.
 * - Window 클래스가 ImGui backend를 직접 초기화하면 안 된다.
 * - 한 프레임에서 ImGui::NewFrame() / ImGui::Render()도 여기에서 한 번만 수행한다.
 *
 * 배경:
 * - 별도 Asset Editor native window에서 ImGui context/backend를 또 만들면 Win32/DX11 backend가 불안정해질 수 있다.
 * - 현재는 안전성을 위해 단일 ImGui context + 메인 DockSpace 패널 구조를 사용한다.
 */
class FEditorImGuiSystem
{
  public:
    bool Initialize(FWindowsWindow *InMainWindow, FD3DDevice *InMainDevice);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void SetActiveWindow(FWindowsWindow *InWindow);
    bool HandleWindowMessage(FWindowsWindow *InWindow, void *hWnd, unsigned int Msg, uintptr_t wParam, intptr_t lParam);

    void MakeCurrentContext() const;

    ImGuiContext   *GetContext() const { return Context; }
    ImFont         *GetTitleBarFont() const { return TitleBarFont; }
    ImFont         *GetWindowControlIconFont() const { return WindowControlIconFont; }
    FWindowsWindow *GetMainWindow() const { return MainWindow; }
    FD3DDevice     *GetMainDevice() const { return MainDevice; }

  private:
    void ApplyEditorColorTheme();
    void ApplyEditorTabStyle();
    void LoadFonts();

  private:
    ImGuiContext   *Context = nullptr;
    FWindowsWindow *MainWindow = nullptr;
    FD3DDevice     *MainDevice = nullptr;
    ImFont         *TitleBarFont = nullptr;
    ImFont         *WindowControlIconFont = nullptr;
    FWindowsWindow *ActiveWindow = nullptr;
    bool            bInitialized = false;
};
