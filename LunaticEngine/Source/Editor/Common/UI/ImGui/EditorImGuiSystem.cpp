#include "PCH/LunaticPCH.h"
#include "Common/UI/ImGui/EditorImGuiSystem.h"

#include "Common/UI/ImGui/EditorImGuiStyleSettings.h"
#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Style/AccentColor.h"
#include "Engine/Render/Device/D3DDevice.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <algorithm>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

namespace
{
    constexpr ImVec4 UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPanelSurfaceActive = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealDockEmpty = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
    constexpr ImVec4 UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
} // namespace

bool FEditorImGuiSystem::Initialize(FWindowsWindow *InMainWindow, FD3DDevice *InMainDevice)
{
    if (bInitialized || !InMainWindow || !InMainDevice || !InMainDevice->GetDevice() || !InMainDevice->GetDeviceContext())
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    Context = ImGui::GetCurrentContext();
    MainWindow = InMainWindow;
    MainDevice = InMainDevice;

    FEditorImGuiStyleSettings::Load();

    ImGuiIO &IO = ImGui::GetIO();
    // Editor Dock/Layout은 사용자 imgui.ini를 복원하지 않고 매 실행마다 기본 레이아웃으로 시작한다.
    // 카메라/프로젝트 설정은 FLevelEditorSettings / FProjectSettings에서 별도로 복원한다.
    IO.IniFilename = nullptr;
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 현재 Editor는 단일 메인 OS Window 안에서 동작하므로 ImGui native viewport 생성을 끈다.
    // 여기서 ViewportsEnable을 켜면 추가 platform window가 생성되어
    // Win32 입력 라우팅과 Active Viewport Context가 어긋날 수 있다.
    IO.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
    ApplyEditorColorTheme();
    ApplyEditorTabStyle();
    LoadFonts();

    ImGui_ImplWin32_Init((void *)InMainWindow->GetHWND());
    ImGui_ImplDX11_Init(InMainDevice->GetDevice(), InMainDevice->GetDeviceContext());

    bInitialized = true;
    return true;
}

void FEditorImGuiSystem::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    MakeCurrentContext();
    PanelTitleUtils::ReleasePanelChromeIconFontForCurrentContext();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(Context);

    Context = nullptr;
    MainWindow = nullptr;
    MainDevice = nullptr;
    TitleBarFont = nullptr;
    WindowControlIconFont = nullptr;
    bInitialized = false;
}

void FEditorImGuiSystem::BeginFrame()
{
    if (!bInitialized || !Context)
    {
        return;
    }

    MakeCurrentContext();
    SetActiveWindow(MainWindow);
    ImGuiIO &IO = ImGui::GetIO();
    if (MainWindow)
    {
        IO.DisplaySize = ImVec2((std::max)(MainWindow->GetWidth(), 1.0f), (std::max)(MainWindow->GetHeight(), 1.0f));
    }
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    if (IO.DeltaTime <= 0.0f)
    {
        IO.DeltaTime = 1.0f / 60.0f;
    }

    ImGui::NewFrame();
}

void FEditorImGuiSystem::EndFrame()
{
    if (!bInitialized || !Context)
    {
        return;
    }

    MakeCurrentContext();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // 단일 Window 기반 Editor 경로에서는 platform viewport 렌더링을 의도적으로 사용하지 않는다.
    // 나중에 Asset Editor를 native window로 분리할 때는 이 옵션만 켜지 말고
    // Window별 WndProc 라우팅과 함께 다시 설계해야 한다.
}

void FEditorImGuiSystem::MakeCurrentContext() const
{
    if (Context)
    {
        ImGui::SetCurrentContext(Context);
    }
}

void FEditorImGuiSystem::SetActiveWindow(FWindowsWindow *InWindow)
{
    if (!bInitialized || !Context || !InWindow || !InWindow->GetHWND())
    {
        return;
    }

    MakeCurrentContext();
    ActiveWindow = InWindow;
    ImGui_ImplWin32_SetActiveWindow((void *)InWindow->GetHWND());
}

bool FEditorImGuiSystem::HandleWindowMessage(FWindowsWindow *InWindow, void *hWnd, unsigned int Msg, uintptr_t wParam, intptr_t lParam)
{
    if (!bInitialized || !Context || !InWindow || !hWnd)
    {
        return false;
    }

    MakeCurrentContext();
    ActiveWindow = InWindow;
    ImGui_ImplWin32_SetActiveWindow(hWnd);
    return ImGui_ImplWin32_WndProcHandler((HWND)hWnd, Msg, (WPARAM)wParam, (LPARAM)lParam) != 0;
}

void FEditorImGuiSystem::ApplyEditorColorTheme()
{
    ImGuiStyle &Style = ImGui::GetStyle();
    Style.Colors[ImGuiCol_WindowBg] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_ChildBg] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_PopupBg] = UnrealPopupSurface;
    Style.Colors[ImGuiCol_TitleBg] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_TitleBgActive] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_TitleBgCollapsed] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_MenuBarBg] = UnrealDockEmpty;
    Style.Colors[ImGuiCol_FrameBg] = UnrealDockEmpty;
    Style.Colors[ImGuiCol_FrameBgHovered] = UnrealPanelSurfaceHover;
    Style.Colors[ImGuiCol_FrameBgActive] = UnrealPanelSurfaceActive;
    Style.Colors[ImGuiCol_CheckMark] = UIAccentColor::Value;
    Style.Colors[ImGuiCol_Button] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_ButtonHovered] = UnrealPanelSurfaceHover;
    Style.Colors[ImGuiCol_ButtonActive] = UnrealPanelSurfaceActive;
    Style.Colors[ImGuiCol_Header] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_HeaderHovered] = UnrealPanelSurfaceHover;
    Style.Colors[ImGuiCol_HeaderActive] = UnrealPanelSurfaceActive;
    Style.Colors[ImGuiCol_Separator] = UnrealDockEmpty;
    Style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(12.0f / 255.0f, 12.0f / 255.0f, 12.0f / 255.0f, 1.0f);
    Style.Colors[ImGuiCol_SeparatorActive] = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
    Style.Colors[ImGuiCol_Border] = UnrealBorder;
    Style.Colors[ImGuiCol_DockingEmptyBg] = UnrealDockEmpty;
}

void FEditorImGuiSystem::ApplyEditorTabStyle()
{
    ImGuiStyle &Style = ImGui::GetStyle();
    Style.TabRounding = (std::max)(Style.TabRounding, 7.0f);
    Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);
    Style.TabBarBorderSize = 0.0f;
    Style.TabBarOverlineSize = 0.0f;
    Style.DockingSeparatorSize = 2.0f;

    Style.Colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    Style.Colors[ImGuiCol_TabHovered] = UnrealPanelSurfaceHover;
    Style.Colors[ImGuiCol_TabSelected] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_TabDimmed] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    Style.Colors[ImGuiCol_TabDimmedSelected] = UnrealPanelSurface;
    Style.Colors[ImGuiCol_TabSelectedOverline] = UnrealDockEmpty;
    Style.Colors[ImGuiCol_TabDimmedSelectedOverline] = UnrealDockEmpty;
}

void FEditorImGuiSystem::LoadFonts()
{
    ImGuiIO    &IO = ImGui::GetIO();
    ImGuiStyle &Style = ImGui::GetStyle();
    Style.WindowPadding.x = (std::max)(Style.WindowPadding.x, 12.0f);
    Style.WindowPadding.y = (std::max)(Style.WindowPadding.y, 10.0f);
    Style.FramePadding.x = (std::max)(Style.FramePadding.x, 8.0f);
    Style.FramePadding.y = (std::max)(Style.FramePadding.y, 5.0f);
    Style.ItemSpacing.x = (std::max)(Style.ItemSpacing.x, 10.0f);
    Style.ItemSpacing.y = (std::max)(Style.ItemSpacing.y, 8.0f);
    Style.CellPadding.x = (std::max)(Style.CellPadding.x, 8.0f);
    Style.CellPadding.y = (std::max)(Style.CellPadding.y, 6.0f);
    Style.CurveTessellationTol = (std::max)(Style.CurveTessellationTol, 0.1f);
    Style.CircleTessellationMaxError = (std::max)(Style.CircleTessellationMaxError, 0.1f);

    const FString               FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
    const std::filesystem::path UIFontPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(FontPath);
    const FString               UIFontPathAbsolute = FPaths::ToUtf8(UIFontPath.lexically_normal().wstring());
    IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
    TitleBarFont = IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

    PanelTitleUtils::EnsurePanelChromeIconFontLoaded();

    if (std::filesystem::exists("C:/Windows/Fonts/segmdl2.ttf"))
    {
        ImFontConfig IconFontConfig{};
        IconFontConfig.PixelSnapH = true;
        WindowControlIconFont = IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segmdl2.ttf", 13.0f, &IconFontConfig);
    }
}
